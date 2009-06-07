#!/bin/sh
bzr log --limit 1 --line $1 | sed -n 's/^\([0-9]*\):.*$/\1/p'
