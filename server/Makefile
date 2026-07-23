# ── FujiBlogger server — Docker helpers ──────────────────────────────────────
#
# Targets:
#   make build        Build the image
#   make run          Run a detached container (generates a fresh JWT_SECRET)
#   make dev          Run in the foreground (logs to stdout, Ctrl-C to stop)
#   make stop         Stop and remove the container
#   make restart      stop + run
#   make logs         Follow container logs
#   make shell        Open a shell in the running container
#   make set-password Interactively set the admin password
#   make push         Push the image to the registry
#   make clean        Stop container and remove the data volume (DESTRUCTIVE)
#
# Override any variable on the command line, e.g.:
#   make run PORT=8080
#   make build IMAGE=myrepo/fujiblogger VERSION=1.2.3
# ─────────────────────────────────────────────────────────────────────────────

IMAGE   ?= superbenk/fujiblogger
VERSION ?= latest
PORT    ?= 8001
VOLUME  ?= "${PWD}/data"
NAME    ?= fujiblogger

# Full image reference used for build / push / run
IMG := $(IMAGE):$(VERSION)

# ─────────────────────────────────────────────────────────────────────────────

.PHONY: build run dev stop restart logs shell set-password push clean help

## Build the Docker image
build:
	docker build -t $(IMG) .

## Run a detached container with a freshly generated JWT_SECRET
run: build
	docker run -d \
		--name $(NAME) \
		-p $(PORT):8000 \
		-v $(VOLUME):/app/data \
		-e JWT_SECRET=$$(openssl rand -hex 32) \
		--restart unless-stopped \
		$(IMG)
	@echo "Server running at http://localhost:$(PORT)"

## Run in the foreground — logs to stdout, Ctrl-C stops and removes the container
dev: build
	docker run --rm \
		--name $(NAME) \
		-p $(PORT):8000 \
		-v $(VOLUME):/app/data \
		-e JWT_SECRET=$$(openssl rand -hex 32) \
		$(IMG)

## Stop and remove the container (data volume is preserved)
stop:
	docker stop $(NAME) && docker rm $(NAME)

## Stop then start a fresh container
restart: stop run

## Follow container logs (Ctrl-C to exit)
logs:
	docker logs -f $(NAME)

## Open an interactive shell in the running container
shell:
	docker exec -it $(NAME) /bin/sh

## Interactively set the admin password inside the running container
set-password:
	docker exec -it $(NAME) .venv/bin/python scripts/set_password.py

## Push the image to the registry
push:
	docker push $(IMG)

## Build and push in one step
release: build push

## Remove the container AND the data volume — ALL POSTS AND CREDENTIALS WILL BE LOST
clean: stop
	docker volume rm $(VOLUME)

## Show this help
help:
	@echo ""
	@echo "Usage: make [target] [VAR=value ...]"
	@echo ""
	@echo "Variables (current values):"
	@echo "  IMAGE   = $(IMAGE)"
	@echo "  VERSION = $(VERSION)"
	@echo "  PORT    = $(PORT)"
	@echo "  VOLUME  = $(VOLUME)"
	@echo "  NAME    = $(NAME)"
	@echo ""
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## /  /' | \
		awk -F: 'BEGIN{OFS=""} {cmd=$$1; sub(/^## /, "", cmd); print "  "cmd}'
	@echo ""
