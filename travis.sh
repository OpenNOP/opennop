#!/bin/bash

function configureTravis {
  mkdir ~/.local
  curl -sSL https://github.com/SonarSource/travis-utils/tarball/latest | tar zx --strip-components 1 -C ~/.local
  source ~/.local/bin/install
}
configureTravis
. installJDK8

if [ "${COVERITY_SCAN_BRANCH}" != 1 ]; then 
  build-wrapper-linux-x86-64 --out-dir build ./build.sh
  
  if [ "${TRAVIS_BRANCH}" == "master" ] && [ "${TRAVIS_PULL_REQUEST}" == "false" ]; then
    sonar-scanner -e -X -Dsonar.login=$SONAR_TOKEN
  else
    
    if [ "${TRAVIS_PULL_REQUEST}" != "false" ]; then   
      sonar-scanner -e -X -Dsonar.analysis.mode=preview \
            -Dsonar.github.pullRequest=$TRAVIS_COMMIT \
            -Dsonar.github.repository=$TRAVIS_REPO_SLUG \
            -Dsonar.github.oauth=$GITHUB_TOKEN \
            -Dsonar.login=$SONAR_TOKEN
    else
      sonar-scanner -e -X -Dsonar.analysis.mode=preview \
            -Dsonar.github.pullRequest=$TRAVIS_PULL_REQUEST \
            -Dsonar.github.repository=$TRAVIS_REPO_SLUG \
            -Dsonar.github.oauth=$GITHUB_TOKEN \
            -Dsonar.login=$SONAR_TOKEN
    fi        
  fi
fi
