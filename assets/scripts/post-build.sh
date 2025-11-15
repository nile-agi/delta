rm -rf ../public/_app;
rm ../public/favicon.svg;
# Keep index.html for server to serve (set_mount_point needs it)
# The server will handle gzip compression automatically if client supports it
# rm ../public/index.html;
