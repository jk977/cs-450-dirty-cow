export scriptdir="$(dirname "$0")"
export appdir="$scriptdir/../app"

echoerr() {
	echo "$@" >&2
}

die() {
	echoerr "$@"
	exit 1
}

dirtycow() {
	local exe="$appdir/dirtycow"

	if [ ! -x "$exe" ]; then
		make -C "$appdir" || die "Failed to compile Dirty COW exploit."
		[ -x "$exe" ]     || die "Executable not found: $exe"
	fi

	"$exe" $@
}
