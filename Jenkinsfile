#!groovy

// Use 'ci-jenkins@somebranch' to pull shared lib from a different branch than the default.
// Default is configured in Jenkins and should be from "stable" tag.
// TODO: Remove the branch name here. once lib support is merged
@Library("ci-jenkins@klaus/pbr-support") import com.swiftnav.ci.*

String dockerFile = "scripts/Dockerfile.jenkins"
String dockerMountArgs = "-v /mnt/efs/refrepo:/mnt/efs/refrepo -v /mnt/efs/buildroot:/mnt/efs/buildroot"

String dockerSecArgs = "--cap-add=SYS_PTRACE --security-opt seccomp=unconfined"
def context = new Context(context: this)
def logger = context.getLogger()
def builder = context.getBuilder()

pipeline {
    // Override agent in each stage to make sure we don't share containers among stages.
    agent any
    options {
        // Make sure job aborts eventually if hanging.
        timeout(time: 8, unit: 'HOURS')
        timestamps()
        // Keep builds for 7 days.
        buildDiscarder(logRotator(daysToKeepStr: '7'))
    }

    parameters {
        string(name: "REF", defaultValue: "master", description: "git ref for piksi_buildroot repo")
        choice(name: "LOG_LEVEL", choices: ['INFO', 'DEBUG', 'WARNING', 'ERROR'])
        // This is only needed until the grpc/protobuf build issue is resolved
        booleanParam(name: "IGNORE_GRPC_FAILURE", defaultValue: true, description: "Ignore failures in branches that use grpc")
        booleanParam(name: "UPLOAD_TO_S3", defaultValue: false; description: "Enable if artifacts should be uploaded to S3")
    }

    environment {
        PIKSI_NON_INTERACTIVE_BUILD="y"
        BR2_DL_DIR="/mnt/efs/buildroot/downloads"
        SSH_AUTH_SOCK="/tmp/ssh_auth_sock"
    }

    stages {
        stage('Build') {
            parallel {
                stage('Release') {
                    when {
                        expression {
                            context.isStageIncluded(name: 'Release')
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "firmware")
                            // Do not fail on build failures in this stage if grpc is present.
                            // This can be removed once EO-282 is fixed.
                            if (ignoreGrpcFailure()) {
                                try {
                                    logger.warning("Ignoring any failures in this stage")
                                    builder.make(target: "image-release-open")
                                    builder.make(target: "image-release-ins")
                                } catch (Exception e) {
                                    logger.error("Ignoring failure since grpc_custom was found")
                                }
                            } else {
                                builder.make(target: "image-release-open")
                                builder.make(target: "image-release-ins")
                            }
                        }
                    }
                    post {
                        success {
                            archivePatterns(
                                context: context,
                                patterns:
                                    ['buildroot/output/images/piksiv3_prod/PiksiMulti-v*.bin',
                                     'buildroot/output/images/piksiv3_prod/PiksiMulti-UNPROTECTED-v*.bin'],
                                addPath: 'v3/prod/',
                                allowEmpty: true)
                        }
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('Internal') {
                    when {
                        expression {
                            context.isStageIncluded()
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "firmware")
                            // Do not fail on build failures in this stage if grpc is present.
                            // This can be removed once EO-282 is fixed.
                            if (ignoreGrpcFailure()) {
                                try {
                                    builder.make(target: "image")
                                } catch (Exception e) {
                                    logger.error("Ignoring failure since grpc_custom was found")
                                }
                            } else {
                                builder.make(target: "image")
                            }
                        }
                    }
                    post {
                        success {
                            archivePatterns(
                                context: context,
                                patterns: ['buildroot/output/images/piksiv3_prod/'],
                                addPath: 'v3/prod/',
                                allowEmpty: true)
                        }
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('Host') {
                    when {
                        expression {
                            context.isStageIncluded()
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs + " " + dockerSecArgs
                        }
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "host-image")
                        }
                    }
                    post {
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('Nano') {
                    when {
                        expression {
                            context.isStageIncluded()
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "nano-config")
                            builder.make(target: "nano-image")
                        }
                    }
                    post {
                        success {
                            archivePatterns(
                                context: context,
                                patterns: ['buildroot/nano_output/images/sdcard.img'],
                                addPath: "nano")
                        }
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('SDK') {
                    when {
                        expression {
                            context.isStageIncluded()
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    environment {
                        BR2_BUILD_SAMPLE_DAEMON = "y"
                        BR2_BUILD_PIKSI_INS_REF = "y"
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "nano-image")
                        }
                    }
                    post {
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('Format Check') {
                    when {
                        expression {
                            context.isStageIncluded()
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        // Ensure that the docker tag in docker_version_tag is same as in Dockerfile.jenkins
                        script {
                            String dockerVersionTag = readFile(file: 'scripts/docker_version_tag').trim()
                            sh("""grep "FROM .*:${dockerVersionTag}" scripts/Dockerfile.jenkins""")
                        }

                        // Enable for newer branches
//                        make(target: "clang-format")
//                        sh('''
//                            | git --no-pager diff --name-only HEAD > /tmp/clang-format-diff
//                            | if [ -s "/tmp/clang-format-diff" ]; then
//                            |     echo "clang-format warning found"
//                            |     git --no-pager diff
//                            |     exit 1
//                            | fi
//                          '''.stripMargin())
                    }
                    post {
                        always {
                            cleanUp()
                        }
                    }
                }
            }
        }
    }
    post {
        always {
            // Common post-run function from ci-jenkins shared libs.
            // Used to e.g. notify slack.
            commonPostPipeline()
        }
    }
}

/**
 * Add the private key for accessing CRL repo.
 * @return
 */
def crlKeyAdd() {
    withCredentials([string(credentialsId: 'github-crl-ssh-key-string', variable: 'GITHUB_CRL_KEY')]) {
        sh("""/bin/bash -e
            set +x
            echo "${GITHUB_CRL_KEY}" > /tmp/github-crl-ssh-key
            set -x
            ssh-agent -a \${SSH_AUTH_SOCK}
            chmod 0600 /tmp/github-crl-ssh-key
            ssh-add /tmp/github-crl-ssh-key
            ssh-add /home/jenkins/.ssh/id_rsa
            ssh-add -l
           """)
    }
}

/**
 * Archive the specified files both to jenkins and to S3
 * @param args
 *   args.patterns: List of file patterns
 * @return
 */
def archivePatterns(Map args) {
    archiveArtifacts(artifacts: args.patterns.join(','))

    if (uploadToS3(args)) {
        args.patterns.each() {
            uploadArtifacts(args + [includePattern: it])
        }
    }
}

/**
 * Upload to S3 if the UPLOAD_TO_S3 parameter is enabled;
 * this may be extended with more criteria later.
 * @return
 */
boolean uploadToS3(Map args=[:]) {
    if (env.UPLOAD_TO_S3 && env.UPLOAD_TO_S3 == "true") {
        return true
    } else {
        return false
    }
}

/**
 * Upload artifacts to S3
 * @param args
 * @return
 */
def uploadArtifacts(Map args) {
    String path = args.path ?: s3Path()
    if (args.addPath) {
        path += args.addPath + "/"
    }

    // Initially, use a bucket separate from travis so we can run both in parallel without conflict.
    String bucket = args.bucket ?: "swiftnav-artifacts-jenkins"
    String pattern = args.includePattern ?: "**"

    // s3Upload is provided by the Jenkins S3 plugin
    s3Upload(
        includePathPattern: pattern,
        bucket: bucket,
        path: path,
        acl: 'BucketOwnerFullControl')
}

/**
 * Determine the path for S3 artifacts within the bucket
 * @return : Path within S3 bucket
 */
String s3Path() {
    String version = env.CHANGE_ID ?: env.GIT_COMMIT
    return "piksi_buildroot/${version}/"
}

/**
 * Clean up some file generated with wrong permissions during the build.
 * This is needed so that a workspace clean is able to remove all files.
 * @return
 */
def cleanUp() {
    // The persistent files get created with root permissions during the build, so
    // the regular 'workspace wipe' is not able to delete them.
    // Remove them explicitly at the end of a build stage.
    cleanWs(
        externalDelete: 'sudo rm -rf %s',
        notFailBuild: true,
        patterns: [[pattern: 'buildroot/host_output/target/fake_persist', type: 'INCLUDE'],
                   [pattern: 'buildroot/host_output/target/persistent', type: 'INCLUDE'],
        ])
}

/**
 * Temporary workaround - check if we should ignore a grpc_custom build failure.
 * @return
 */
boolean ignoreGrpcFailure() {
    if (fileExists('package/grpc_custom/grpc_custom.mk')) {
        if (env.IGNORE_GRPC_FAILURE && env.IGNORE_GRPC_FAILURE == "true") {
            return true
        }
        return false
    }
}
