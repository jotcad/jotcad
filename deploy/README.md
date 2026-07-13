# JotCAD Deployment Configuration

This directory contains the Ansible playbooks, inventory configurations, and systemd service templates used to build, package, and deploy the JotCAD Mesh network nodes, routers, bridges, and webcam servers across the network.

## Directory Responsibilities

- **Inventory Management**: Defines the target host groups (`webcam_servers` and `local_services`) in `hosts.ini`.
- **Systemd Service Templating**: Provides configuration templates for automatic startup and lifecycle management of JotCAD components under `templates/`.
- **Automated Deployments**: Playbooks to copy files, install dependencies, compile C++ nodes, and manage systemd services on remote nodes.

## Playbooks

- **[deploy.yml](deploy.yml)**: The main deployment playbook. Manages the installation of the Zenoh Router (`zenohd`), Zenoh Remote API WebSocket Bridge (`zenoh-bridge-remote-api`), Dual Camera UX dashboard, and webcam node, as well as the compilation of C++ geometry operators on all hosts.
- **[local_deploy.yml](local_deploy.yml)**: Playbook specialized for deploying and managing services targeting only the local Odroid host.
- **[webcam_deploy.yml](webcam_deploy.yml)**: Playbook for updating the JotCAD webcam service.

## Templates (`templates/`)

- **`jotcad-router.service.j2`**: Template for starting `zenohd`.
- **`jotcad-bridge.service.j2`**: Template for starting the Zenoh Remote API WebSocket Bridge.
- **`jotcad-webcam.service.j2`**: Template for the webcam collector node.
- **`jotcad-ux.service.j2`**: Template for serving the Dual Camera UX dashboard.
- **`jotcad-geo-op.service.j2`**: Template for managing C++ geometry operator services.
