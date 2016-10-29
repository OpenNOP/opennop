#!/bin/bash

function configureTravis {
  mkdir ~/.local
  curl -sSL https://github.com/SonarSource/travis-utils/tarball/latest | tar zx --strip-components 1 -C ~/.local
  source ~/.local/bin/install
}
configureTravis
. installJDK8

if [ "${COVERITY_SCAN_BRANCH}" != 1 ]; then 
  ./autogen.sh && ./configure && make && cd opennopdrv && make && cd ..; 
fi

sonar-scanner -Dsonar.login=$SONAR_TOKEN
