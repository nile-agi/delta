use std::io::BufRead;
use std::sync::atomic::{AtomicBool, Ordering};

// Keep the transport boundary testable without starting a desktop window.
pub fn forward_lines(
    reader: impl BufRead,
    abort: &AtomicBool,
    mut send: impl FnMut(String) -> bool,
) -> Result<(), String> {
    let mut lines = reader.lines();
    loop {
        if abort.load(Ordering::Relaxed) {
            return Ok(());
        }
        let Some(line) = lines.next() else { break };
        let line = line.map_err(|error| format!("Response stream failed: {error}"))?;
        let completed = line.trim_end() == "data: [DONE]";
        if !send(line) || completed {
            return Ok(());
        }
    }
    if abort.load(Ordering::Relaxed) {
        Ok(())
    } else {
        Err("Response stream ended before completion. Please try again.".into())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{self, BufReader, Cursor, Read};

    struct Timeout;
    impl Read for Timeout {
        fn read(&mut self, _: &mut [u8]) -> io::Result<usize> {
            Err(io::Error::new(io::ErrorKind::TimedOut, "test timeout"))
        }
    }

    #[test]
    fn done_releases_the_stream_before_another_read() {
        let first = Cursor::new(b"data: hello\n\ndata: [DONE]\r\n\r\n");
        let mut lines = Vec::new();
        let result = forward_lines(
            BufReader::new(first.chain(Timeout)),
            &AtomicBool::new(false),
            |line| {
                lines.push(line);
                true
            },
        );
        assert!(result.is_ok());
        assert_eq!(lines.last().unwrap(), "data: [DONE]");
    }

    #[test]
    fn timeout_and_incomplete_streams_are_errors() {
        assert!(
            forward_lines(BufReader::new(Timeout), &AtomicBool::new(false), |_| true)
                .unwrap_err()
                .contains("test timeout")
        );
        assert!(forward_lines(
            Cursor::new(b"data: partial\n"),
            &AtomicBool::new(false),
            |_| true
        )
        .unwrap_err()
        .contains("before completion"));
    }

    #[test]
    fn cancellation_and_closed_channels_stop_forwarding() {
        let abort = AtomicBool::new(true);
        assert!(forward_lines(BufReader::new(Timeout), &abort, |_| panic!("cancelled")).is_ok());
        assert!(
            forward_lines(Cursor::new(b"data: late\n"), &abort, |_| panic!(
                "cancelled"
            ))
            .is_ok()
        );
        assert!(forward_lines(
            Cursor::new(b"data: first\ndata: second\n"),
            &AtomicBool::new(false),
            |_| false,
        )
        .is_ok());
    }
}
