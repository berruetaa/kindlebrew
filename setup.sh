#!/bin/sh

KPM="/var/local/kmc/bin/kpm"
SU="/var/local/kmc/bin/su"
REPO="https://ve.uy/repo"

run_kpm() {
    "$SU" -c "$KPM --fbink $*"
    return $?
}

run_kpm add-repo "$REPO"
ADD_RC=$?

if [ "$ADD_RC" -eq 0 ]; then
    run_kpm update
    run_kpm list-repo
else
    if [ -x /var/local/kmc/bin/fbink ]; then
        /var/local/kmc/bin/fbink -x 1 -y 1 -S 2 \
            "Kindlebrew: no se pudo agregar el repo (rc=$ADD_RC)" >/dev/null 2>&1 || true
    fi
fi

sleep 4
exit "$ADD_RC"
