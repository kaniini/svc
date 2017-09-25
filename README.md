# svc

`svc` is a proposed service manager and process supervisor to replace OpenRC in Alpine.  It is presently under construction.


## key components

* `svc-supervise` is a lightweight process supervisor that can be managed by the service manager using [libnv][libnv] IPC.

* `svc-manager` is a manager which loads service declarations and manages a collection of svc-supervise processes to monitor each
  service.

* `svc-init` initializes the system and manages the root `svc-manager` process.

  [libnv]: http://github.com/kaniini/libnv


## `libnv`

libnv is a port of the FreeBSD and Solaris IPC library, `libnv`.  It is used to transfer messages between processes and acts
as the core IPC primitive.


## `svc-supervise`

The `svc-supervise` process supervisor monitors processes.  Typically these processes run as children of the supervisor, but
support for monitoring PID files is pending.


## `svc-manager`

The `svc-manager` service manager manages process supervisors and handles dependency resolution for a given target.  A `svc-manager`
process may be managed by a parent `svc-manager` process.


## `svc-init`

The `svc-init` process brings up the system and starts the first `svc-manager` process.


## design goals

* Avoid the use of dynamically allocated memory where possible.

* Recognize and properly handle containers, so that svc-init may serve as a capable init for dockerized alpine deployments.

* Play nice when run under systemd-nspawn.

* If in doubt, look at other process supervisors and service managers such as s6 for guidance.
