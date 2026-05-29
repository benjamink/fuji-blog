#!/bin/sh
# Entrypoint for the FujiBlogger Docker container.
#
# Runs as root so it can fix ownership on the mounted data volume, then drops
# to the unprivileged fujiblog user before starting the server.  This handles
# the case where the named volume was initialised by a previous container run
# (or a different image build) and therefore carries root:root ownership.
set -e

chown -R fujiblog:fujiblog /app/data

exec runuser -u fujiblog -- "$@"
