# OH MY CAL

OMC aims to consolidate the steps required to build, test, and deploy calibration pipelines and other software suites maintained by engineers at the Space Telescope Science Institute.

## Requirements

- Linux, or MacOS (Darwin)
- cmake
- libcurl
- rsync

# Installation

Download the source code

```shell
git clone https://github.com/jhunkeler/ohmycal.git
cd ohmycal
```

Create and enter the build directory

```shell
mkdir build
cd build
```

Run cmake

```shell
cmake .. -DCMAKE_INSTALL_PREFIX="$HOME/programs/ohmycal"
```

Compile and install ohmycal

```shell
make
make install
```

# Configuration

## Environment variables

| Name                         | Purpose                                               | 
|------------------------------|-------------------------------------------------------|
| TMPDIR                       | Change default path to store temporary data           | 
| OMC_SYSCONFDIR               | Change default path to search for configuration files | 
| OMC_JF_ARTIFACTORY_URL       | Artifactory service URL (ending in `/artifactory`)    | 
| OMC_JF_ACCESS_TOKEN          | Artifactory Access Token                              | 
| OMC_JF_USER                  | Artifactory username                                  | 
| OMC_JF_PASSWORD              | Artifactory password                                  | 
| OMC_JF_SSH_KEY_PATH          | Path to SSH public key file                           |
| OMC_JF_SSH_PASSPHRASE        | Password associated with SSH public key file          | 
| OMC_JF_CLIENT_CERT_CERT_PATH | Path to OpenSSL cert files                            | 
| OMC_JF_CLIENT_CERT_KEY_PATH  | OpenSSL key file (in cert path)                       | 
| OMC_JF_REPO                  | Artifactory "generic" repository to write to          | 

# Variable expansion

## Template strings

Template strings can be accessed using the `{{ subject.key }}` notation in any Ohmycal configuration file.

| Name                       | Purpose                            |
|----------------------------|------------------------------------|
| meta.name                  | Delivery name                      |
| meta.version               | Delivery version                   |
| meta.codename              | Delivery codename                  |
| meta.mission               | Delivery mission                   |
| meta.python                | Python version (e.g. 3.11)         |
| meta.python_compact        | Python (e.g. 311)                  |
| info.time_str_epoch        | UNIX Epoch timestamp               |
| info.release_name          | Rendered delivery release name     |
| info.build_name            | Rendered delivery build name       |
| info.build_number          | Rendered delivery build number     |
| storage.tmpdir             | Ohymcal temp directory             |
| storage.delivery_dir       | Ohmycal delivery output directory  |
| storage.conda_artifact_dir | Ohmycal conda package directory    |
| storage.wheel_artifact_dir | Ohmycal wheel package directory    |
| storage.build_sources_dir  | Ohmycal sources directory          |
| conda.installer_name       | Conda distribution name            |
| conda.installer_version    | Conda distribution version         |
| conda.installer_platform   | Conda target platform              |
| conda.installer_arch       | Conda target architecture          |
| conda.installer_baseurl    | Conda installer URL                |
| system.arch                | System CPU Architecture            |
| system.platform            | System Platform (OS)               |
| deploy.repo                | Artifactory destination repository |

The template engine also provides an interface to environment variables using the `{{ env:VARIABLE_NAME }}` notation.

```ini
[meta]
name = {{ env:MY_DYNAMIC_DELIVERY_NAME }}
version = {{ env:MY_DYNAMIC_DELIVERY_VERSION }}
python = {{ env:MY_DYNAMIC_PYTHON_VERSION }}
```

## Runtime strings

Alternatively, one may expand environment variables using the `${VARIABLE_NAME}` notation.
Note: Environment variable expansion is not rendered in `[test:*].script` keys.

```ini
[meta]
name = ${MY_DYNAMIC_DELIVERY_NAME}
version = ${MY_DYNAMIC_DELIVERY_VERSION}
python = ${MY_DYNAMIC_PYTHON_VERSION}
```

# Delivery files

## Sections

All configuration section names and keys are _case-sensitive_.

### meta

| Key      | Type    | Purpose                                                         | Required |
|----------|---------|-----------------------------------------------------------------|----------|
| mission  | String  | Mission rule directory to use                                   | Y        |
| name     | String  | Name of the delivery                                            | Y        |
| version  | String  | Version of delivery                                             | Y        |
| codename | String  | Release code name (HST-only)                                    | N        |
| rc       | Integer | Release candidate level (i.e. delivery iteration)               | Y        |
| final    | Boolean | Toggle final delivery logic (possibly deprecated in the future) | N        |
| based_on | String  | A conda environment YAML file/URL to use as a baseline          | N        |
| python   | String  | Version of Python to base delivery on                           | Y        |

### conda

| Key                | Type   | Purpose                    | Required |
|--------------------|--------|----------------------------|----------|
| installer_name     | String | Conda distribution name    | Y        |
| installer_version  | String | Conda distribution version | Y        |
| installer_platform | String | Conda target platform      | Y        |
| installer_arch     | String | Conda target architecture  | Y        |
| installer_baseurl  | String | Conda installer URL        | Y        |
| conda_packages     | List   | Conda packages to install  | N        |
| pip_packages       | List   | Pypi packages to install   | N        |

### runtime

Environment variables exported are _global_ to all programs executed by ohmycal. There is no limit to the number of environment variables that can be set.

| Key                    | Type   | Purpose                                                    | Required |
|------------------------|--------|------------------------------------------------------------|----------|
| _arbitrary name_       | String | Export environment variables to build and test environment | N        |
| _artitrary_name ... N_ | -      | -                                                          | -        |

### test:_name_

Sections starting with `test:` will be used during the testing phase of the ohmycal pipeline. Where the value of `name` following the colon is an arbitrary value, and only used for reporting which test-run is executing. Section names must be unique.

| Key          | Type   | Purpose                                               | Required |
|--------------|--------|-------------------------------------------------------|----------|
| build_recipe | String | Git repository path to package's conda recipe         | N        |
| repository   | String | Git repository path or URL to clone                   | Y        |
| version      | String | Git commit or tag to check out                        | Y        |
| runtime      | List   | Export environment variables specific to test context | Y        |
| script       | List   | Body of a shell script that will execute the tests    | Y        |

### deploy:artifactory:_name_

Sections starting with `deploy:artifactory:` will define the upload behavior of build and test artifacts to Artifactory. Where the value of `name` is an arbitrary value, and only used for reporting. Section names must be unique.

| Key   | Type   | Purpose                                | Required |
|-------|--------|----------------------------------------|----------|
| files | List   | `jf`-compatible wildcard path          | Y        |
| dest  | String | Remote artifactory path to store files | Y        |

# Mission files

Mission rules are defined in the `OMC_SYCONFDIR/mission` directory. Each mission configuration file shares the same name as the directory. To create a new mission, `example`, the directory structure will be as follows:

```text
OMC_SYSCONFDIR/
    mission/
        example/
            example.ini
            supporting_text.md.in
```

## Sections 

### meta

| Key              | Type   | Purpose                                                   | Required |
|------------------|--------|-----------------------------------------------------------|----------|
| release_fmt      | String | A string containing one or more release format specifiers | Y        |
| build_name_fmt   | String | -                                                         | Y        |
| build_number_fmt | String | -                                                         | Y        |

#### Release format specifiers

| Formatter | Purpose                          |
|-----------|----------------------------------|
| %n        | Delivery name                    |
| %c        | Delivery codename                |
| %r        | Delivery RC level                |
| %R        | Delivery RC level (final-aware)  |
| %v        | Delivery version                 |
| %m        | Mission name                     |
| %P        | Python version (i.e. 3.11)       |
| %p        | Compact Python version (i.e. 311 |
| %a        | System CPU Architecture          |
| %o        | System Platform (OS)             |
| %t        | UNIX Epoch timestamp             |

### template:filename

Sections starting with `template:` will make use of the built-in template engine, replacing any strings in `filename`, relative to the mission directory, with expanded data values. The output will be written to `destination`.

```ini
# example.ini
[template:supporting_text.md.in]
destination = {{ storage.delivery_dir }}/output_filename.md
```
