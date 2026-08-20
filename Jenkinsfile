pipeline {
  agent any

  options {
    timestamps()
    disableConcurrentBuilds()
    buildDiscarder(logRotator(numToKeepStr: '20'))
    skipDefaultCheckout(true)
  }

  parameters {
    string(name: 'GIT_REF', defaultValue: '*/master',
           description: 'Git branch pattern or refs/tags/<tag>')
  }

  environment {
    REPOSITORY_URL = 'https://github.com/bogdan-tirzioru/BusAnalizer.git'
    CUBEIDE_HEADLESS = '/opt/st/stm32cubeide_1.11.0/headless-build.sh'
    GNU_ARM_BIN = '/opt/st/stm32cubeide_1.11.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.10.3-2021.10.linux64_1.0.100.202210260954/tools/bin'
    PROJECT_NAME = 'USB_test'
  }

  stages {
    stage('Clean and checkout') {
      steps {
        deleteDir()
        checkout([$class: 'GitSCM',
          branches: [[name: params.GIT_REF]],
          extensions: [[$class: 'CloneOption', depth: 0, noTags: false, shallow: false]],
          userRemoteConfigs: [[
            url: env.REPOSITORY_URL,
            refspec: '+refs/heads/*:refs/remotes/origin/* +refs/tags/*:refs/tags/*'
          ]]
        ])
        sh '''#!/usr/bin/env bash
set -euo pipefail
git rev-parse HEAD | tee git-commit.txt
git describe --always --dirty --tags | tee git-describe.txt
'''
      }
    }

    stage('Verify toolchain') {
      steps {
        sh '''#!/usr/bin/env bash
set -euo pipefail
test -x "$CUBEIDE_HEADLESS"
test -x "$GNU_ARM_BIN/arm-none-eabi-gcc"
"$GNU_ARM_BIN/arm-none-eabi-gcc" --version | head -n 1
'''
      }
    }

    stage('Build Debug') {
      steps {
        timeout(time: 15, unit: 'MINUTES') {
          sh '''#!/usr/bin/env bash
set -euo pipefail
rm -rf "$WORKSPACE/.cubeide-workspace"
"$CUBEIDE_HEADLESS" -data "$WORKSPACE/.cubeide-workspace" \
  -import "$WORKSPACE/$PROJECT_NAME" \
  -cleanBuild "$PROJECT_NAME/Debug" 2>&1 | tee cubeide-build.log
'''
        }
      }
    }

    stage('Inspect firmware') {
      steps {
        sh '''#!/usr/bin/env bash
set -euo pipefail
elf=$(find "$PROJECT_NAME/Debug" -maxdepth 1 -type f -name '*.elf' -print -quit)
test -n "$elf"
"$GNU_ARM_BIN/arm-none-eabi-size" "$elf" | tee firmware-size.txt
sha256sum "$elf" | tee firmware.sha256
'''
      }
    }
  }

  post {
    always {
      archiveArtifacts artifacts: 'cubeide-build.log,git-commit.txt,git-describe.txt,firmware-size.txt,firmware.sha256,USB_test/Debug/*.elf,USB_test/Debug/*.map,USB_test/Debug/*.hex,USB_test/Debug/*.bin',
                       allowEmptyArchive: true,
                       fingerprint: true
    }
  }
}
