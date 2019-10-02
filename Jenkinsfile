#!groovy

// Use 'ci-jenkins@someref' to pull shared lib from a different branch/tag than the default.
// Default is configured in Jenkins and should be from "stable" tag.
@Library("ci-jenkins@jbangelo/STAR-816-move-pfwp-to-jenkins") import com.swiftnav.ci.*

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
        booleanParam(name: "FORCE_TOOLCHAIN_BUILD", defaultValue: false, description: "Force a toolchain build");
        booleanParam(name: "FORCE_SDK_BUILD", defaultValue: false, description: "Force an SDK build");
    }

    environment {
        PIKSI_NON_INTERACTIVE_BUILD="y"
        // Cache package downloads here
        BR2_DL_DIR="/mnt/efs/buildroot/downloads"
        // Needed to add multiple private keys
        SSH_AUTH_SOCK="/tmp/ssh_auth_sock"
        JENKINS_SKIP_AWS_AUTH_CHECK="y"
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
                    environment {
                        VARIANT = "release"
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "image")
                        }
                    }
                    post {
                        success {
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/output/release/images/piksiv3_prod/PiksiMulti-v*.bin'],
                                        addPath: 'v3/prod'
                                )
                            }
                        }
                        always {
                            cleanUp()
                        }
                    }
                }

                stage('Unprotected') {
                    when {
                        expression {
                            context.isStageIncluded(name: 'Unprotected')
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    environment {
                        VARIANT = "unprotected"
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "image")
                        }
                    }
                    post {
                        success {
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/output/unprotected/images/piksiv3_prod/PiksiMulti-UNPROTECTED-v*.bin'],
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
                            context.isStageIncluded(name: 'Internal')
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    environment {
                        VARIANT = "internal"
                        HITL_API_BUILD_TYPE = "buildroot_pull_request"
                        HITL_VIEWER_BUILD_TYPE = "buildroot_pr"
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {

                            builder.make(target: "image")

                            if (needTagArtifacts(context: context)) {
                              builder.make(target: "export-toolchain")
                              context.archivePatterns(
                                patterns: [
                                  'piksi_br_toolchain.txz'
                                ]
                              )
                            }

                            context.archivePatterns(
                                patterns: [
                                    'buildroot/output/internal/images/piksiv3_prod/PiksiMulti*',
                                    'buildroot/output/internal/images/piksiv3_prod/uImage*',
                                    'buildroot/output/internal/images/piksiv3_prod/boot.bin',
                                ],
                                addPath: 'v3/prod',
                            )

                            if (context.isPrPush()) {
                                sh('./scripts/gen-requirements-yaml internal')
                                createPrDescription(context: context)
                                context.archivePatterns(
                                    patterns: [
                                        'requirements.yaml',
                                        'pr_description.yaml',
                                    ],
                                )

                                sh('git diff')

                                hitl.triggerForPr() // this generates metrics.yaml
                                context.archivePatterns(
                                    patterns: [
                                        'ci-jenkins/metrics.yaml',
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
                            context.isStageIncluded(name: 'Host')
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs + " " + dockerSecArgs
                        }
                    }
                    environment {
                        VARIANT = "host"
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "image")
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
                            context.isStageIncluded(name: 'Nano')
                        }
                    }
                    agent {
                        dockerfile {
                            filename dockerFile
                            args dockerMountArgs
                        }
                    }
                    environment {
                        VARIANT = "nano"
                    }
                    steps {
                        stageStart()
                        gitPrep()
                        crlKeyAdd()

                        script {
                            builder.make(target: "image")
                        }
                    }
                    post {
                        success {
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/output/nano/images/sdcard.img'],
                                        addPath: 'nano/evt1'
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
                            context.isStageIncluded(name: 'SDK') && needSdkBuild(context: context)
                        }
                    }
                    agent {
                        node('docker.m')
                    }
                    environment {
                        VARIANT = "sdk"
                    }
                    steps {
                        stageStart()
                        script {
                          def tag = 'piksi_br_sdk_build'
                          def branch = sdkBranchName(context: context)
                          def wsDir = pwd()
                          def tempDir = pwd(tmp:true)
                          context.logger.info("SDK build: Copying Dockerfile from wsDir=${wsDir} to tempDir=${tempDir}")
                          sh("cp ${wsDir}/scripts/Dockerfile.sdk ${tempDir}/Dockerfile")
                          dir(tempDir) {
                            context.logger.info("SDK build: running docker build (branch=${branch}, tag=${tag})...")
                            sh("docker build --build-arg branch=${branch} -t ${tag} .")
                            deleteDir()
                          }
                          sh("mkdir -p buildroot/output/sdk/images")
                          sh("docker run --name ${tag}-run --rm ${tag} ls -l buildroot/output/sdk/images")
                          sh("docker run --name ${tag}-run --rm -v ${wsDir}/buildroot/output/sdk:/output ${tag} cp -vr buildroot/output/sdk/images/ /output/")
                        }
                    }
                    post {
                        success {
                            script {
                                context.archivePatterns(
                                        patterns: ['buildroot/output/sdk/images/piksiv3_prod/PiksiMulti-SDK-v*.bin'],
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
        patterns: [[pattern: 'buildroot/output/host/target/fake_persist', type: 'INCLUDE'],
                   [pattern: 'buildroot/output/host/target/persistent', type: 'INCLUDE'],
                   [pattern: 'buildroot/output/host/target/persistent', type: 'INCLUDE'],
                   [pattern: 'buildroot/output/sdk/images', type: 'INCLUDE'],
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

def needSdkBuild(Map args=[:]) {

    assert args.context

    def branchName = gitBranchName(context: args.context)
    def isRelBranch = branchName ==~ /^v.*-release$/

    args.context.logger.info("needSdkBuild: branchName=${branchName}, isRelBranch=${isRelBranch}")

    if (args.context.pipe.params.FORCE_SDK_BUILD) {
      args.context.logger.info("needSdkBuild: true; FORCE_SDK_BUILD")
      return true
    }

    if (args.context.isPrPush()) {
      args.context.logger.info("needSdkBuild: false; isPrPush")
      return false
    }

    if (!isRelBranch) {

      args.context.logger.info("needSdkBuild: false; !isRelBranch")
      return false
    }

    if (!gitHeadHasTag(context: args.context)) {

      args.context.logger.info("needSdkBuild: false; !gitHeadHasTag")
      return false
    }

    return true
}

def needTagArtifacts(Map args=[:]) {

    assert args.context

    if (args.context.pipe.params.FORCE_TOOLCHAIN_BUILD) {

      args.context.logger.info("needTagArtifacts: true; FORCE_TOOLCHAIN_BUILD was set")
      return true
    }

    args.context.logger.info(
      "Found !FORCE_TOOLCHAIN_BUILD")

    if (args.context.isPrPush()) {

      args.context.logger.info("needTagArtifacts: false; isPrPush")
      return false
    }

    if (!args.context.isTagPush()) {

      args.context.logger.info("needTagArtifacts: false; !isTagPush")
      return false
    }

    args.context.logger.info("needTagArtifacts: true; !isPrPush; isTagPush")

    return true
}
