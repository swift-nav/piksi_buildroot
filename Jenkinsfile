#!groovy

// Use 'ci-jenkins@someref' to pull shared lib from a different branch/tag than the default.
// Default is configured in Jenkins and should be from "stable" tag.

// TODO: Remove branch after ci-jenkins PR is merged/tagged
@Library("ci-jenkins@klaus/pr-class") import com.swiftnav.ci.*

String dockerFile = "scripts/Dockerfile.jenkins"
String dockerMountArgs = "-v /mnt/efs/refrepo:/mnt/efs/refrepo -v /mnt/efs/buildroot:/mnt/efs/buildroot"

String dockerSecArgs = "--cap-add=SYS_PTRACE --security-opt seccomp=unconfined"

def context = new Context(context: this)
context.setRepo("piksi_buildroot")

def builder = context.getBuilder()
def logger = context.getLogger()
def hitl = new SwiftHitl(context: context)
hitl.update()

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
//                stage('s3-test') {
//                    when {
//                        // Run only when specifically included via the STAGE_INCLUDE parameter.
//                        expression {
//                            context.isStageIncluded(name: 'S3-test', includeOnEmpty: false)
//                        }
//                    }
//                    agent {
//                        dockerfile {
//                            filename dockerFile
//                            args dockerMountArgs
//                        }
//                    }
//                    steps {
//                        stageStart()
//                        gitPrep()
//                        crlKeyAdd()
//
//                        // create a dummy file that we can save later
//                        sh("mkdir -p a/b/c && touch dummy_file.txt a/b/c/PiksiMulti_1.bin a/b/c/PiksiMulti_2.bin a/b/c/somefile.txt")
//                    }
//                    post {
//                        success {
//                            script {
//                                context.archivePatterns(
//                                        patterns: ['a/b/c/*.bin'],
//                                        addPath: 'dummy/addme'
//                                )
//                            }
//                        }
//                        always {
//                            cleanUp()
//                        }
//                    }
//                }
//
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
                    environment {
                        GENERATE_REQUIREMENTS=1
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
                            createPrDescription(context: context)
                            context.archivePatterns(
                                patterns: [
                                    'buildroot/output/images/piksiv3_prod/PiksiMulti*',
                                    'buildroot/output/images/piksiv3_prod/uImage*',
                                    'buildroot/output/images/piksiv3_prod/boot.bin',
                                ],
                                addPath: 'v3/prod',
                            )
                            context.archivePatterns(
                                patterns: [
                                    'requirements.yaml',
                                    'pr_description.yaml',
                                ],
                            )
                            if (context.isPrPush()) {
                                hitl.triggerForPr()
                                context.archivePatterns(
                                    patterns: [
                                        'metrics.yaml',
                                    ],
                                )
                            }
                            hitl.addComments()
                        }
                    }
                    post {
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
                                        addPath: 'nano/evt1'
                                )
                            }
                        }
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('Toolchain') {
                    when {
                        expression {
                            context.isStageIncluded(name: 'Toolchain', includeOnEmpty: false)
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
                            builder.make(target: "export-toolchain")
                        }
                    }
                    post {
                        success {
                            script {
                                context.archivePatterns(
                                        patterns: ['piksi_br_toolchain.txz'],
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

                        script {
                            // Ensure that the docker tag in docker_version_tag is same as in Dockerfile.jenkins
                            // (no longer needed when switching to buildroot-base image pulled from docker-recipes)
                            //String dockerVersionTag = readFile(file: 'scripts/docker_version_tag').trim()
                            //sh("""grep "FROM .*:${dockerVersionTag}" scripts/Dockerfile.jenkins""")
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

def createPrDescription(Map args=[:]) {
    assert args.context
    assert args.context.isPrPush()

    def pr = new PullRequest(context: args.context)
    pr.update()

    writeFile(
        file: "pr_description.yaml",
        text: """
            |---
            |commit:
            |  sha: ${pr.data.head.sha}
            |  message: ${pr.data.title}
            |  range: ${pr.data.base.sha}..${pr.data.head.sha}
            |pr:
            |  number: ${pr.getNumber()}
            |  source_branch: ${pr.data.head.ref}
            |  sha: ${pr.data.head.sha}
            |  source_slug: ${pr.org}/${pr.repo}
            |target:
            |  branch: ${pr.data.base.ref}
            |  slug: ${pr.org}/${pr.repo}
            |test_result: 0
            |tag:
            |""".stripMargin()
    )
}

def triggerHitl(Map args=[:]) {
    assert args.context
    assert args.context.isPrPush()

    /**
     *   ./scripts/travis_pr_describe.sh
     *   ./scripts/generate_requirements.sh
     *   ./scripts/hitl_smoke_test
     *
     *   # Push a comment to Slack #github channel
     *   SLACK_CHANNEL=github \
     *     ./scripts/comment.sh
     */

    def pr = new PullRequest(context: args.context)
    pr.update()

    withCredentials([usernamePassword(
        credentialsId: 'github-access-token',
        usernameVariable: 'HITL_API_GITHUB_USER',
        passwordVariable: 'HITL_API_GITHUB_TOKEN',
    )]) {
        withCredentials([string(credentialsId: 'github-access-token-secretText',
            variable: 'GITHUB_COMMENT_TOKEN')]) {
            sh("""#!/bin/bash -ex
                |export GITHUB_PULL_REQUEST=${pr.getNumber()}
                |export REPO=piksi_buildroot
                |./scripts/hitl_smoke_test_jenkins.py
                |""".stripMargin())


        }
    }
}

/**
 * This replaces the scripts/comment.sh script; it sends a message to Slack for tag/branch builds.
 * For PR-triggered builds, it adds a comment to Github PRs and sets the PR check to 'pending'.
 * @param args
 * @return
 */
def hitlComments(Map args=[:]) {
    assert args.context

    String buildVersion = args.context.gitDescribe()
    String buildPath = "${args.context.repo}/${buildVersion}"
    String hitlViewerUrl = "https://gnss-analysis.swiftnav.com"
    String bucket = args.context.s3Bucket()

    // TODO: Remove the 'true' which is here only for testing
    if (true || args.context.isTagPush() || args.context.isBranchPush(branches: ['master', 'v*-release'])) {
        slackSend(
            color: 'good',
            channel: 'github',
            message: "${buildPath} https://console.aws.amazon.com/s3/home?region=us-west-2&bucket=${bucket}&prefix=${buildPath}"
        )
    }

    if (args.context.isPrPush()) {
        def pr = new PullRequest(context: args.context)
        pr.update()
        String comment = """
            |## ${buildVersion}
            |+ [Artifacts (HITL Dashboard Page)](${hitlViewerUrl}/artifacts/build_type=buildroot_pr&name_filter=${buildVersion})
            |+ [Artifacts (S3 / AWS Console)](https://console.aws.amazon.com/s3/home?region=us-west-2&bucket=${bucket}&prefix=${buildPath}/)
            |+ s3://${bucket}/${buildPath}
            |""".stripMargin()
        pr.addComment(message: comment)

        pr.postStatus(
            targetUrl: "${hitlViewerUrl}/metrics_preset=pass_fail&scenario=live-roof-650-townsend-post&build_type=buildroot_pr&firmware_versions=${buildVersion}",
            state: 'pending',
            description: "Waiting for HITL tests to be run and complete",
            statusContext: 'hitl/pass-fail',
        )
    }
}
