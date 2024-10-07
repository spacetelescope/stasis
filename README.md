[![CMake on multiple platforms](https://github.com/spacetelescope/stasis/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/spacetelescope/stasis/actions/workflows/cmake-multi-platform.yml) [![Documentation Status](https://readthedocs.org/projects/stasis-docs/badge/?version=latest)](https://stasis-docs.readthedocs.io/en/latest/?badge=latest)

STASIS consolidates the steps required to build, test, and deploy calibration pipelines and other software suites maintained by engineers at the Space Telescope Science Institute.

# Requirements

- Linux, or MacOS (Darwin)
- cmake
- libcurl
- libxml2
- rsync

# Installation

Download the source code

```shell
git clone https://github.com/spacetelescope/stasis.git
cd stasis
```

Create and enter the build directory

```shell
mkdir build
cd build
```

Run cmake

```shell
cmake .. -DCMAKE_INSTALL_PREFIX="$HOME/programs/stasis"
```

Compile and install stasis

```shell
make
make install
export PATH="$HOME/programs/stasis/bin:$PATH"
```

# Quickstart

## Step 1: Create a mission

Missions definitions live in STASIS's `etc/mission` directory. Let's create a new one specifically geared toward generic data analysis tools. You may override the path to the `etc` directory by setting the `STASIS_SYSCONFDIR` environment variable to different location.

```shell
mkdir $HOME/programs/stasis/etc/missions/mymission
touch $HOME/programs/stasis/etc/missions/mymission/mymission.ini
```

Now populate the new data analysis mission configuration. Refer to the [release formatters](#release-formatters) section to see a list of what each `%` formatter does.

```ini
[meta]
release_fmt = %n-%v-%r-py%p-%o-%a
; e.g. mydelivery-1.2.3-1-py312-linux-x86_64

build_name_fmt = %n-%v
; e.g. mydelivery-1.2.3

build_number_fmt = %v.%r
; e.g. 1.2.3.1
```

Text files containing STASIS template strings can be stored at the same level as the mission configuration, and rendered to anywhere inside the output directory. This will can you time if you plan to release for multiple platforms and architectures.

```ini
[template:readme.md.in]
destination = {{ storage.delivery_dir }}/README-{{ info.release_name }}.md
; e.g [..]/output/delivery/README-mydelivery-1.2.3-1-py312-linux-x86_64.md

[template:Dockerfile.in]
destination = {{ storage.build_docker_dir }}/Dockerfile
; e.g [..]/build/docker/Dockerfile
```

## Step 2: Create a delivery configuration

STASIS's configuration parser does not distinguish input files by extension, so `mydelivery.ini`, `mydelivery.cfg`, `mydelivery.txt`, and `abc123.zyx987` are all perfectly valid file names.

All deliveries require a `[meta]` section. Here global metadata such as the delivery's `name`, `version`, and any programs/dependencies that make up the deliverable.

```ini
[meta]
mission = mymission
name = mydelivery
version = 2024.3.1
rc = 1
python = 3.12
```

The `[conda]` section instructs STASIS how to obtain the conda installer of your choice, and defines the packages to be installed into the delivery's release environment.

```ini
[conda]
; e.g. Download Miniforge3-23.11.0-0 for the current system platform and architecture
installer_name = Miniforge3
installer_version = 23.11.0-0
installer_platform = {{env:STASIS_CONDA_PLATFORM}}
installer_arch = {{env:STASIS_CONDA_ARCH}}
installer_baseurl = https://github.com/conda-forge/miniforge/releases/download/{{conda.installer_version}}

conda_packages =
    qt>=5.0
    
pip_packages =
    our_cool_program
    our_other_cool_program
```

Create some test cases for packages.

```ini
[test:our_cool_program]
version = 1.2.3
repository = https://github.com/org/our_cool_program
script_setup =
    pip install -e '.[test]'
script =
    pytest -fEsx \
        --basetemp="{{ func:basetemp_dir() }}" \
        --junitxml="{{ func:junitxml_file() }}" \
        tests/

[test:our_other_cool_program]
version = 4.5.6
repository = https://github.com/org/our_other_cool_program
script_setup =
    pip install -e '.[test]'
script =
    pytest -fEsx \
        --basetemp="{{ func:basetemp_dir() }}" \
        --junitxml="{{ func:junitxml_file() }}" \
        tests/
```

## Step 3: Run STASIS

```shell
stasis mydelivery.ini
```

# Configuration

## Command Line Options

| Long Option                | Short Option | Purpose                                                        |
|:---------------------------|:------------:|:---------------------------------------------------------------|
| --help                     |      -h      | Display usage statement                                        |
| --version                  |      -V      | Display program version                                        |
| --continue-on-error        |      -C      | Allow tests to fail                                            |
| --config ARG               |    -c ARG    | Read STASIS configuration file                                 |
| --cpu-limit ARG            |    -l ARG    | Number of processes to spawn concurrently (default: cpus - 1)  |
| --pool-status-interval ARG |     n/a      | Report task status every n seconds (default: 30)               |
| --python ARG               |    -p ARG    | Override version of Python in configuration                    |
| --verbose                  |      -v      | Increase output verbosity                                      |
| --unbuffered               |      -U      | Disable line buffering                                         |
| --update-base              |     n/a      | Update conda installation prior to STATIS environment creation |
| --fail-fast                |     n/a      | On test error, terminate all tasks                             |
| --overwrite                |     n/a      | Overwrite an existing release                                  |
| --no-docker                |     n/a      | Do not build docker images                                     |
| --no-artifactory           |     n/a      | Do not upload artifacts to Artifactory                         |
| --no-testing               |     n/a      | Do not execute test scripts                                    |
| --no-parallel              |     n/a      | Do not execute tests in parallel                               |
| --no-rewrite               |     n/a      | Do not rewrite paths and URLs in output files                  |
| DELIVERY_FILE              |     n/a      | STASIS delivery file                                           |

## Environment variables

| Name                            | Purpose                                                                 | 
|---------------------------------|-------------------------------------------------------------------------|
| TMPDIR                          | Change default path to store temporary data                             |
| STASIS_ROOT                     | Change default path to write STASIS's data                              |
| STASIS_SYSCONFDIR               | Change default path to search for configuration files                   | 
| STASIS_CPU_COUNT                | Number of available CPUs                                                |
| STASIS_GH_TOKEN                 | GitHub API token<br/>(Scope: "Contents" repository permissions (write)) |
| STASIS_JF_ARTIFACTORY_URL       | Artifactory service URL (ending in `/artifactory`)                      | 
| STASIS_JF_ACCESS_TOKEN          | Artifactory Access Token                                                | 
| STASIS_JF_USER                  | Artifactory username                                                    | 
| STASIS_JF_PASSWORD              | Artifactory password                                                    | 
| STASIS_JF_SSH_KEY_PATH          | Path to SSH public key file                                             |
| STASIS_JF_SSH_PASSPHRASE        | Password associated with SSH public key file                            | 
| STASIS_JF_CLIENT_CERT_CERT_PATH | Path to OpenSSL cert files                                              | 
| STASIS_JF_CLIENT_CERT_KEY_PATH  | OpenSSL key file (in cert path)                                         | 
| STASIS_JF_REPO                  | Artifactory "generic" repository to write to                            | 

## Main configuration (stasis.ini)

The default path to the configuration file is `[CMAKE_INSTALL_PREFIX]/etc/stasis/stasis.ini`. You may override this by setting the `STASIS_SYSCONFDIR` environment variable to a path that points elsewhere. 

### Sections

#### default 

| Name                           |  Type   | Purpose                                                              |
|--------------------------------|:-------:|----------------------------------------------------------------------|
| continue_on_error              | Boolean | Keep going even if a test fails                                      |
| always_update_base_environment | Boolean | Update all packages in the base to the latest release                |
| conda_fresh_start              | Boolean | Remove conda installation during initialization                      |
| conda_install_prefix           | String  | Install conda in a custom prefix path                                |
| conda_packages                 |  List   | Conda packages to be installed/overridden in the `base` environment  |
| pip_packages                   |  List   | Python packages to be installed/overridden in the `base` environment |
| conda_staging_url              | String  | URL to conda channel                                                 |

### jfrog_cli_download

| Name           |  Type  | Purpose                                                               |
|----------------|:------:|-----------------------------------------------------------------------|
| url            | String | Base URL of JFrog CLI release server                                  |
| project        | String | Product identifier (i.e. `jfrog-cli`)                                 |
| version_series | String | Product version series (i.e. `v2-jf`)                                 |
| version        | String | Product version to install. `[RELEASE]` downloads the latest version. |
| filename       | String | Product file name (i.e. `jf`)                                         |

### deploy:artifactory

| Name |  Type  | Purpose                                              |
|------|:------:|------------------------------------------------------|
| url  | String | Set artifactory service URL (ending in /artifactory) |
| repo | String | Set artifactory repository                           |


# Delivery configuration

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

Environment variables exported are _global_ to all programs executed by stasis. There is no limit to the number of environment variables that can be set.

| Key                    | Type   | Purpose                                                    | Required |
|------------------------|--------|------------------------------------------------------------|----------|
| _arbitrary name_       | String | Export environment variables to build and test environment | N        |
| _artitrary_name ... N_ | -      | -                                                          | -        |

### test:_name_

Sections starting with `test:` will be used during the testing phase of the stasis pipeline. Where the value of `name` following the colon is an arbitrary value, and only used for reporting which test-run is executing. Section names must be unique.

| Key          | Type    | Purpose                                                     | Required |
|--------------|---------|-------------------------------------------------------------|----------|
| disable      | Boolean | Disable `script` execution (`script_setup` always executes) | N        |
| parallel     | Boolean | Execute test block in parallel (default) or sequentially    | N        |
| build_recipe | String  | Git repository path to package's conda recipe               | N        |
| repository   | String  | Git repository path or URL to clone                         | Y        |
| version      | String  | Git commit or tag to check out                              | Y        |
| runtime      | List    | Export environment variables specific to test context       | Y        |
| script_setup | List    | Body of a shell script that will install dependencies       | N        |
| script       | List    | Body of a shell script that will execute the tests          | Y        |

### deploy:artifactory:_name_

Sections starting with `deploy:artifactory:` will define the upload behavior of build and test artifacts to Artifactory. Where the value of `name` is an arbitrary value, and only used for reporting. Section names must be unique.

| Key   | Type   | Purpose                                | Required |
|-------|--------|----------------------------------------|----------|
| files | List   | `jf`-compatible wildcard path          | Y        |
| dest  | String | Remote artifactory path to store files | Y        |

### deploy:docker

The `deploy:docker` section controls how Docker images are created, when a `Dockerfile` is present in the `build_docker_dir`.

| Key               | Type   | Purpose                                      | Required |
|-------------------|--------|----------------------------------------------|----------|
| registry          | String | Docker registry to use                       | Y        |
| image_compression | String | Compression program (with arguments)         | N        |
| build_args        | List   | Values passed to `docker build --build-args` | N        |
| tags              | List   | Docker image tag(s)                          | Y        |
# Variable expansion

## Template strings

Template strings can be accessed using the `{{ subject.key }}` notation in any STASIS configuration file.

| Name                        | Purpose                                                                                                                 |
|-----------------------------|-------------------------------------------------------------------------------------------------------------------------|
| meta.name                   | Delivery name                                                                                                           |
| meta.version                | Delivery version                                                                                                        |
| meta.codename               | Delivery codename                                                                                                       |
| meta.mission                | Delivery mission                                                                                                        |
| meta.python                 | Python version (e.g. 3.11)                                                                                              |
| meta.python_compact         | Python (e.g. 311)                                                                                                       |
| info.time_str_epoch         | UNIX Epoch timestamp                                                                                                    |
| info.release_name           | Rendered delivery release name                                                                                          |
| info.build_name             | Rendered delivery build name                                                                                            |
| info.build_number           | Rendered delivery build number                                                                                          |
| storage.tmpdir              | Ohymcal temp directory                                                                                                  |
| storage.delivery_dir        | STASIS delivery output directory                                                                                        |
| storage.results_dir         | STASIS test results directory                                                                                           |
| storage.conda_artifact_dir  | STASIS conda package directory                                                                                          |
| storage.wheel_artifact_dir  | STASIS wheel package directory                                                                                          |
| storage.build_sources_dir   | STASIS sources directory                                                                                                |
| storage.build_docker_dir    | STASIS docker directory                                                                                                 |
| conda.installer_name        | Conda distribution name                                                                                                 |
| conda.installer_version     | Conda distribution version                                                                                              |
| conda.installer_platform    | Conda target platform                                                                                                   |
| conda.installer_arch        | Conda target architecture                                                                                               |
| conda.installer_baseurl     | Conda installer URL                                                                                                     |
| system.arch                 | System CPU Architecture                                                                                                 |
| system.platform             | System Platform (OS)                                                                                                    |
| deploy.docker.registry      | Docker registry                                                                                                         |
| deploy.jfrog.repo           | Artifactory destination repository                                                                                      |
| workaround.conda_reactivate | Reinitialize the conda runtime environment.<br/>Use this after calling `conda install` from within a `[test:*].script`. |

The template engine also provides an interface to environment variables using the `{{ env:VARIABLE_NAME }}` notation.

```ini
[meta]
name = {{ env:MY_DYNAMIC_DELIVERY_NAME }}
version = {{ env:MY_DYNAMIC_DELIVERY_VERSION }}
python = {{ env:MY_DYNAMIC_PYTHON_VERSION }}
```

## Template Functions

Template functions can be accessed using the `{{ func:NAME(ARG,...) }}` notation.

| Name                          | Arguments | Purpose                                                          |
|-------------------------------|-----------|------------------------------------------------------------------|
| get_github_release_notes_auto | n/a       | Generate release notes for all test contexts                     |
| basetemp_dir                  | n/a       | Generate directory path to test block's temporary data directory |
| junitxml_file                 | n/a       | Generate directory path and file name for test result file       |


# Mission files

Mission rules are defined in the `STASIS_SYCONFDIR/mission` directory. Each mission configuration file shares the same name as the directory. To create a new mission, `example`, the directory structure will be as follows:

```text
STASIS_SYSCONFDIR/
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

#### Release formatters

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
