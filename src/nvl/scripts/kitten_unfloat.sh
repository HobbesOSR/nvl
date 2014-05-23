#!/bin/sh

# Git submodules checkout repos as "floating branches"
# The dance below unfloats the checkout so that it can be pushed
# the upstream to master.

git branch temp
git checkout temp
git branch -f master temp
git checkout master
git branch -d temp

# git push ssh://software.sandia.gov/git/kitten
