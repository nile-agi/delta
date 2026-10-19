"""HTTP server used by the isolated wrapper integration fixtures."""
from http.server import ThreadingHTTPServer
from socketserver import TCPServer


class LoopbackHTTPServer(ThreadingHTTPServer):
    def server_bind(self):
        # HTTPServer resolves getfqdn() during bind. Numeric loopback fixtures
        # must remain available when the host's name service is slow or absent.
        TCPServer.server_bind(self)
        self.server_name, self.server_port = self.server_address[:2]
