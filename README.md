# cnld
<code>Code 'n Load</code> device daemon. Install this program on your device.

## Prerrequisites:
    sudo apt update
    sudo apt install git libsystemd-dev autossh python-pip python-systemd python3-systemd python-sdnotify python3-sdnotify wiringpi

## Prerrequisites for Python applications:
    sudo pip install wiringpi sdnotify

## Instalation:
1. Clone repository.

    git clone https://github.com/codenload/cnld.git && cd cnld

1. Run installation. If you want to register the device, you will be asked for your <code>Code 'n Load</code> username and password. You can leave default configuration values, you can change them later from the Control Panel.

    make install
