# Piksi v3 Buildroot

[Buildroot](https://buildroot.org/) for building Linux for Piksi v3. The resulting images go on the piksi's SD card. Run `./build.sh` to build locally (requires `gcc`).

## Travis CI and build artifacts

Pull requests and pushing to the `swift-nav` branch will trigger a `upload_artifacts.sh` in successful Travis builds, which uploads the output images to the [piksi-buildroot-images](https://console.aws.amazon.com/s3/home?region=us-west-2#&bucket=piksi-buildroot-images&prefix=) S3 bucket.