# webos-vncserver
An somewhat hacky VNC server for WebOS.

Requires root privileges.

# Usage

Install provided package, launch VNC Server app, adjust configuration (mind
default enabled password), press "Save settings", and switch "Running" on.

You should then be able to connect via a VNC client of your choice, on port 5900.

Here it is, running on a "headless" TV motherboard, being accessed via a VNC mobile app.

![Demo](./img/demo.jpg?raw=true)

Alternatively, `webos-vncserver` can be launched via CLI for debugging/testing:
```
# ./webos-vncserver
```

This will immediately launch VNC server, regardless of autostart options.

For extra logging `VNCSERVER_DEBUG=1` environment variable can be set.

In order to make the service register on Luna bus when running via CLI
`LS_SERVICE_NAMES=org.webosbrew.vncserver.service` environment variable needs to
be set. This is usually not required, unless debugging some deeper
frontend-service integration, since the service shall be started automatically
by `ls-hubd` when called by frontend.

## Caveats
- This does not capture any hardware-accelerated video surfaces, only the UI layers.
- Capture may conflict with other applications or webOS services.
  If video display hangs or crashes, try stopping relevant services (eg. via
  `pkill -f captureservice`) before starting the service up/connecting using VNC.
  Notable conflicting services:
    - `piccap`: `hyperion-webos`
    - webOS 5.x+: `captureservice`

- `tigervnc`: "React too big" error can be alleviated by adding `Autoselect=0`
  option to command line
- webOS limited network throughput (100mbps over ethernet) requires use of JPEG,
  make sure your VNC clients supports it.
- All configuration changes cause client disconnects.

## Keybindings

- SUPER ("Windows") button is HOME
- Right mouse button is BACK
- Mouse scroll should properly be interpreted as scrollwheel

## Luna service control
**(advanced users only)**

VNC server is implemented as a proper Luna service. In order to make it run as
root `elevate-service` script of Homebrew Channel needs to be used after install
or upgrade - this is done automatically on first "VNC Server" app launch.

Service can be controlled using Luna service bus calls:

* `luna://org.webosbrew.vncserver.service/start` - start up the service
* `luna://org.webosbrew.vncserver.service/stop` - stop the service
* `luna://org.webosbrew.vncserver.service/status` - return current status and
  configuration
* `luna://org.webosbrew.vncserver.service/quit` - shut down the service
  altogether
* `luna://org.webosbrew.vncserver.service/configure` - change configuration,
  supports:
    * `width` - capture width
    * `height` - capture height
    * `framerate` - framerate limit
    * `autostart` - start automatically on boot (assumes correct Homebrew
      Channel autostart configuration)
    * `password` - basic authentication - can be set to empty string for no
      authentication

As usual - all these commands can be issued using `luna-send` command like so:
```sh
luna-send -n 1 'luna://org.webosbrew.vncserver.service/configure' '{"password": "test"}'
luna-send -n 1 'luna://org.webosbrew.vncserver.service/start' '{}'
```

# Building
## Service
To cross-compile for WebOS, you will [need a
toolchain](https://github.com/openlgtv/buildroot-nc4/releases/tag/webos-b17b4cc).

```sh
cmake -S . -B build && cmake --build build --target webos-vncserver --target capture_gm --target capture_halgal
```

This should have produced a `build/service/` directory. Copy it over to your TV and run `./webos-vncserver` as root!

## Frontend
Configuration frontend is based on web technologies - in order to build it you
will need NodeJS (18.x) and use:
```sh
npm install
npm run build
```

A convenience shortcut for native build here is provided as:

```sh
npm run build-native
```

Then, in order to package and deploy an app, use:

```sh
npm run package
npm run deploy
npm run launch
```
