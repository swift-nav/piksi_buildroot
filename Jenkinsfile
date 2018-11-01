#!groovy

// Use 'ci-jenkins@somebranch' to pull shared lib from a different branch than the default.
// Default is configured in Jenkins and should be from "stable" tag.
@Library("ci-jenkins") import com.swiftnav.ci.*

String dockerFile = "scripts/Dockerfile.jenkins"
String dockerMountArgs = "-v /mnt/efs/refrepo:/mnt/efs/refrepo -v /mnt/efs/buildroot:/mnt/efs/buildroot"

String dockerSecArgs = "--cap-add=SYS_PTRACE --security-opt seccomp=unconfined"
def context = new Context(context: this)
def builder = context.getBuilder()
def logger = context.getLogger()

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
        choice(name: "LOG_LEVEL", choices: ['info', 'debug', 'warning', 'error'])
        booleanParam(name: "UPLOAD_TO_S3",
                defaultValue: true,
                description: "Artifacts get uploaded to S3 if build is a tag/PR/master/release build. "
                        + "Uncheck this flag to prevent all S3 uploading.")
    }

    environment {
        PIKSI_NON_INTERACTIVE_BUILD="y"
        // Cache package downloads here
        BR2_DL_DIR="/mnt/efs/buildroot/downloads"
        // Needed to add multiple private keys
        SSH_AUTH_SOCK="/tmp/ssh_auth_sock"
    }

    stages {
        stage('Build') {
            parallel {
                // This is a stage that can be used to quickly try some changes.
                // It won't run if STAGE_INCLUDE is empty. To run it, set the STAGE_INCLUDE parameter
                // to "s3-test".
                stage('s3-test') {
                    when {
                        // Run only when specifically included via the STAGE_INCLUDE parameter.
                        expression {
                            context.isStageIncluded(name: 'S3-test', includeOnEmpty: false)
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

                        // create a dummy file that we can save later
                        sh("date > dummy_file.txt")
                    }
                    post {
                        success {
                            archivePatterns(
                                    context: context,
                                    patterns:
                                            ['dummy_file.txt'],
                                    addPath: 'v3/dummy',
                                    allowEmpty: false)
                        }
                        always {
                            cleanUp()
                        }
                    }
                }

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
                            builder.make(target: "image-release-open")
                            builder.make(target: "image-release-ins")
                        }
                    }
                    post {
                        success {
                            archivePatterns(
                                context: context,
                                patterns:
                                    ['buildroot/output/images/piksiv3_prod/PiksiMulti-v*.bin',
                                     'buildroot/output/images/piksiv3_prod/PiksiMulti-UNPROTECTED-v*.bin'],
                                addPath: 'v3/prod',
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
                            builder.make(target: "image")
                        }
                    }
                    post {
                        success {
                            archivePatterns(
                                context: context,
                                patterns: ['buildroot/output/images/piksiv3_prod/'],
                                addPath: 'v3/prod',
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
                            builder.make(target: "firmware")
                            builder.make(target: "image")
                            builder.make(target: "sdk")
                        }
                    }
                    post {
                        success {
                            archivePatterns(
                                    context: context,
                                    patterns: ['piksi_sdk.txz'],
                                    addPath: "v3/prod")
                        }
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
                            builder.make(target: "clang-format")
                            sh('''
                                | git --no-pager diff --name-only HEAD > /tmp/clang-format-diff
                                | if [ -s "/tmp/clang-format-diff" ]; then
                                |     echo "clang-format warning found"
                                |     git --no-pager diff
                                |     exit 1
                                | fi
                              '''.stripMargin())
                        }
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
            script {
                context.slackNotify()
                context.postCommon()
            }
        }
    }
}

/**
 * Add the private key for accessing CRL repo.
 * Consider moving this off to the shared libs.
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
 * Archive the specified files both to jenkins and to S3.
 * Consider moving this off to the shared libs.
 * @param args
 *   args.patterns: List of file patterns
 * @return
 */
def archivePatterns(Map args) {
    archiveArtifacts(artifacts: args.patterns.join(','))

    if (shouldUploadToS3(args)) {
        args.patterns.each() {
            uploadArtifactsToS3(args + [includePattern: it])
        }
    }
}

/**
 * Upload to S3 if the UPLOAD_TO_S3 parameter is enabled or does not exist;
 * this may be extended with more criteria later.
 *
 * Note that S3 upload is skipped for non-tag/pr/release builds.
 * @return
 */
boolean shouldUploadToS3(Map args=[:]) {
    if (env.UPLOAD_TO_S3 && env.UPLOAD_TO_S3 == "false") {
        return false
    } else {
        return true
    }
}

/**
 * Upload artifacts to S3. Determine the bucket and path based on whether this
 * is a PR, a branch, or a tag push.
 * @param args
 *   args.context
 *   args.path
 *   args.bucket
 *   args.addPath
 * @return
 */
def uploadArtifactsToS3(Map args) {
    // Initially, use a bucket separate from travis so we can run both in parallel without conflict.
    assert args.context

    boolean upload = false
    String bucket
    String path = "piksi_buildroot/"

    if (args.context.isTagPush()) {
        bucket = "swiftnav-artifacts-jenkins"
        path += context.gitDescribe() + "/"
        upload = true
    } else {
        if (args.context.isPrPush()) {
            bucket = "swiftnav-artifacts-pull-requests-jenkins"
            path += "pr-" + args.context.prNumber + "/" + args.context.gitDescribe() + "/"
            upload = true
        } else {
            // TODO remove the 'klaus.*-release' which is here to test uploads
            if (args.context.isBranchPush(branches: ['master', 'v.*-release', 'klaus.*-release'])) {
                bucket = "swiftnav-artifacts-jenkins"
                path += args.context.branchName() + "/" + args.context.gitDescribe() + "/"
                upload = true
            } else {
                logger.info("Neither a tag, PR, or master/release branch push - not publishing artifacts to S3")
                upload = false
            }
        }
    }

    // You can override the bucket/path via arg
    if (args.bucket) {
        bucket = args.bucket
    }
    if (args.path) {
        path = args.path
    }

    // Append to the path if specified
    if (args.addPath) {
        path += args.addPath + "/"
    }

    String pattern = args.includePattern ?: "**"

    if (upload && shouldUploadToS3()) {
        // s3Upload is provided by the Jenkins S3 plugin
        s3Upload(
                includePathPattern: pattern,
                bucket: bucket,
                path: path,
                acl: 'BucketOwnerFullControl')
    }
}

/**
 * Clean up some file generated with wrong permissions during the build.
 * This is needed so that a workspace clean is able to remove all files.
 * @return
 */
def cleanUp() {
    /**
     * The persistent files get created with root permissions during the build, so
     * the regular 'workspace wipe' is not able to delete them.
     * Remove them explicitly at the end of a build stage.
     * Fail build if cleanup fails, so that we can find out what had created
     * the undeletable files/dirs.
     */
    cleanWs(
        externalDelete: 'sudo rm -rf %s',
        patterns: [[pattern: 'buildroot/host_output/target/fake_persist', type: 'INCLUDE'],
                   [pattern: 'buildroot/host_output/target/persistent', type: 'INCLUDE'],
        ])
}
