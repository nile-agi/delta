use std::io::{Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::time::{Duration, Instant};

#[derive(Debug, PartialEq, Eq)]
pub enum StartupOutcome {
    Ready,
    TimedOut,
    Interrupted,
}

pub fn wait_for_server(
    port: u16,
    timeout: Duration,
    interrupted: impl Fn() -> bool,
) -> StartupOutcome {
    let deadline = Instant::now() + timeout;
    let address = SocketAddr::from(([127, 0, 0, 1], port));
    while Instant::now() < deadline && !interrupted() {
        let remaining = || deadline.saturating_duration_since(Instant::now());
        let slice = || {
            remaining()
                .min(Duration::from_millis(250))
                .max(Duration::from_millis(1))
        };
        if let Ok(mut stream) = TcpStream::connect_timeout(&address, slice()) {
            let _ = stream.set_write_timeout(Some(slice()));
            if stream
                .write_all(b"GET /api/props HTTP/1.0\r\nHost: localhost\r\n\r\n")
                .is_ok()
            {
                let _ = stream.set_read_timeout(Some(slice()));
                let mut response = [0; 64];
                if let Ok(n) = stream.read(&mut response) {
                    if response[..n].starts_with(b"HTTP/1.")
                        && !interrupted()
                        && Instant::now() < deadline
                    {
                        return StartupOutcome::Ready;
                    }
                }
            }
        }
        std::thread::sleep(remaining().min(Duration::from_millis(50)));
    }
    if interrupted() {
        StartupOutcome::Interrupted
    } else {
        StartupOutcome::TimedOut
    }
}

/// Ask this instance's local model API to release its child before killing the backend.
pub fn unload_backend(port: u16) -> bool {
    let deadline = Instant::now() + Duration::from_secs(3);
    let address = SocketAddr::from(([127, 0, 0, 1], port));
    let Ok(mut stream) = TcpStream::connect_timeout(&address, Duration::from_millis(250)) else {
        return false;
    };
    let remaining = || deadline.saturating_duration_since(Instant::now()).max(Duration::from_millis(1));
    let _ = stream.set_write_timeout(Some(remaining()));
    if stream.write_all(b"POST /api/models/unload HTTP/1.0\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}").is_err() {
        return false;
    }
    let _ = stream.set_read_timeout(Some(remaining()));
    let mut response = [0; 64];
    stream.read(&mut response).map(|n| {
        response[..n].starts_with(b"HTTP/1.1 200 ") || response[..n].starts_with(b"HTTP/1.0 200 ")
    }).unwrap_or(false)
}

#[cfg(test)]
mod tests {
    #[test]
    fn shutdown_unloads_owned_models_before_the_backend_is_killed() {
        use std::io::{Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let mut request = [0; 4096];
            let size = stream.read(&mut request).unwrap();
            assert!(String::from_utf8_lossy(&request[..size]).starts_with("POST /api/models/unload "));
            stream.write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}").unwrap();
        });
        assert!(super::unload_backend(port));
        server.join().unwrap();
    }
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn interruption_is_distinct_from_a_startup_timeout() {
        assert_eq!(
            format!("{:?}", wait_for_server(1, Duration::from_secs(30), || true)),
            "Interrupted"
        );
    }

    #[test]
    fn dead_backend_stops_without_waiting_for_the_deadline() {
        let start = Instant::now();
        assert_eq!(
            wait_for_server(1, Duration::from_secs(30), || true),
            StartupOutcome::Interrupted
        );
        assert!(start.elapsed() < Duration::from_millis(100));
    }

    #[test]
    fn accepts_http_response_but_not_a_socket_closed_without_data() {
        for valid in [false, true] {
            let listener = TcpListener::bind("127.0.0.1:0").unwrap();
            let port = listener.local_addr().unwrap().port();
            std::thread::spawn(move || {
                if let Ok((mut stream, _)) = listener.accept() {
                    let mut request = [0; 128];
                    let _ = stream.read(&mut request);
                    if valid {
                        let _ = stream.write_all(b"HTTP/1.0 200 OK\r\n\r\n{}");
                    }
                }
            });
            assert_eq!(
                wait_for_server(port, Duration::from_millis(150), || false),
                if valid {
                    StartupOutcome::Ready
                } else {
                    StartupOutcome::TimedOut
                }
            );
        }
    }

    #[test]
    fn stalled_socket_cannot_extend_the_startup_deadline() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        std::thread::spawn(move || {
            let _stream = listener.accept().unwrap();
            std::thread::sleep(Duration::from_millis(500));
        });
        let start = Instant::now();
        assert_eq!(
            wait_for_server(port, Duration::from_millis(50), || false),
            StartupOutcome::TimedOut
        );
        assert!(start.elapsed() < Duration::from_millis(250));
    }
}
