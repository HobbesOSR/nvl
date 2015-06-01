#!/bin/sh

git submodule init
git submodule update

# Change from free-floating branch to master branch
cd src/nvl/kitten
git checkout master
cd ../../../

# Change from free-floating branch to devel branch
cd src/nvl/palacios
git checkout devel
cd ../../../

# Change from free-floating branch to devel branch
cd src/nvl/pisces/palacios
git checkout devel
cd ../../../../

# Change from free-floating branch to devel branch
cd src/nvl/pisces/pisces
git checkout master
cd ../../../../

# Change from free-floating branch to devel branch
cd src/nvl/pisces/petlib
git checkout master
cd ../../../../

# Change from free-floating branch to devel branch
cd src/nvl/pisces/xpmem
git checkout master
cd ../../../../

# Change from free-floating branch to devel branch
cd src/nvl/pisces/hobbes
git checkout master
cd ../../../../
