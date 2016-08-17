# Piksi v3 Buildroot

[Buildroot](https://buildroot.org/) for building Linux for Piksi v3. The resulting images go on the piksi's SD card. Run `./build.sh` to build locally (requires `gcc`).

## Travis CI and build artifacts

Pushing to the `swift-nav` branch will trigger a script in Travis that uploads the built images to the [piksi-buildroot-images](https://console.aws.amazon.com/s3/home?region=us-west-2#&bucket=piksi-buildroot-images&prefix=) S3 bucket. This requires AWS keys, which ["are not available to pull requests from forks due to the security risk of exposing such information to unknown code"](https://docs.travis-ci.com/user/environment-variables/#Encrypted-Variables), so this will not happen on Travis builds for PRs from personal forks of the `swift-nav` repo.