#!/bin/sh

# Small script to split out major/minor/micro versions for API version checks.
VERSION=$(./git-version-gen .tarball-version)

case "$1" in
	-ma)
		echo $VERSION | cut -f1 -d.
		;;
	-mi)
		echo $VERSION | cut -f2 -d. | cut -f1 -d~
		;;
	-mc)
		micro_version=$(echo $VERSION | cut -f3 -d. | cut -f1 -d-)
		if test -z "$micro_version"; then
			echo 0
		else
			echo $micro_version
		fi
		;;
	*)
		echo "Usage: $0 [-ma|-mi|-mc]"
		exit 1
		;;
esac
