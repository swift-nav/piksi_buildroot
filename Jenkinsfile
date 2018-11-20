#!groovy

// Use 'ci-jenkins@someref' to pull shared lib from a different branch/tag than the default.
// Default is configured in Jenkins and should be from "stable" tag.

//TODO: Remove the s3fix tag before merge to master
@Library("ci-jenkins@s3fix") import com.swiftnav.ci.*

String dockerFile = "scripts/Dockerfile.jenkins"
String dockerMountArgs = "-v /mnt/efs/refrepo:/mnt/efs/refrepo -v /mnt/efs/buildroot:/mnt/efs/buildroot"

String dockerSecArgs = "--cap-add=SYS_PTRACE --security-opt seccomp=unconfined"

def context = new Context(context: this)
context.setRepo("piksi_buildroot")

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
                        sh("mkdir -p a/b/c && touch dummy_file.txt a/b/c/PiksiMulti_1.bin a/b/c/PiksiMulti_2.bin a/b/c/somefile.txt")
                    }
                    post {
                        success {
                            script {
                                context.archivePatterns(
                                        patterns: ['a/b/c/*.bin'],
                                        addPath: 'dummy/addme'
                                )
                            }
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
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/output/images/piksiv3_prod/PiksiMulti-v*.bin',
                                                   'buildroot/output/images/piksiv3_prod/PiksiMulti-UNPROTECTED-v*.bin'],
                                        addPath: 'v3/prod'
                                )
                            }
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
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/output/images/piksiv3_prod/PiksiMulti*',
                                                   'buildroot/output/images/piksiv3_prod/uImage*'],
                                        addPath: 'v3/prod'
                                )
                            }
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
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/nano_output/images/sdcard.img'],
                                        addPath: 'v3/prod'
                                )
                            }
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
                            script {
                                context.archivePatterns(
                                        patterns: ['piksi_sdk.txz'],
                                        addPath: 'v3/prod'
                                )
                            }
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
