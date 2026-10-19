#!/usr/bin/env bash
#
# generate-qt-headers.sh -- build a usable Qt 6 include tree out of the qtbase
# source tree.
#
# Why this exists
# ---------------
# The only Qt 6 build available in this environment is the *runtime* part of
# the official Qt binaries (shared libraries + plugins) shipped inside the
# PySide6 wheels on PyPI.  Those wheels contain no headers, no CMake package
# files and no moc/uic/rcc, and this machine has no working package manager,
# so the matching public headers are produced here from the qtbase sources of
# the *same* Qt version (see setup-toolchain.sh).
#
# The script performs the four jobs Qt's own build tooling (syncqt + the
# configure step) would normally do:
#
#   1. forwarding headers   <QtModule>/<Header>  and  <QtModule>/private/<Header>_p.h
#   2. forwarding headers   <QtModule>/<QClass>  (class include shorthands)
#   3. generated headers    <QtModule>/qt<module>exports.h
#   4. generated headers    <QtModule>/qt<module>-config.h (+ the private twin)
#
# Job (4) needs the value of every Qt feature the runtime was built with.  The
# runtime is a full desktop Linux build, so features are enabled unless they
# are listed in EXCLUDE_FEATURES below (platform specific features, licensing
# or tracing backends that a stock desktop build does not have).  If a feature
# turns out to be wrong the compiler or the linker says so: a header that
# enables a feature the library lacks produces a link error for the missing
# symbol, a header that disables a feature the library has produces a
# "no member named ..." error.  Both are deterministic to repair.
#
# Usage: generate-qt-headers.sh <qtbase-source-dir> <output-include-dir>
#
set -euo pipefail

QTSRC=${1:?usage: $0 <qtbase-source-dir> <output-include-dir>}
OUT=${2:?usage: $0 <qtbase-source-dir> <output-include-dir>}

[ -d "$QTSRC/src/corelib" ] || { echo "qtbase sources not found in $QTSRC" >&2; exit 1; }

# module directory -> Qt module name
MODULES="corelib:QtCore gui:QtGui widgets:QtWidgets network:QtNetwork sql:QtSql \
concurrent:QtConcurrent xml:QtXml testlib:QtTest printsupport:QtPrintSupport"

# export macro + generated header names for every module we expose
#   <module>:<config module name>:<define infix>
# (an empty config module name means "this module has no configure.cmake")
declare -A CFGMOD INFIX
CFGMOD[QtCore]=core;        INFIX[QtCore]=CORE
CFGMOD[QtGui]=gui;          INFIX[QtGui]=GUI
CFGMOD[QtWidgets]=widgets;  INFIX[QtWidgets]=WIDGETS
CFGMOD[QtNetwork]=network;  INFIX[QtNetwork]=NETWORK
CFGMOD[QtSql]=sql;          INFIX[QtSql]=SQL
CFGMOD[QtConcurrent]=;      INFIX[QtConcurrent]=CONCURRENT
CFGMOD[QtXml]=xml;          INFIX[QtXml]=XML
CFGMOD[QtTest]=testlib;     INFIX[QtTest]=TESTLIB
CFGMOD[QtPrintSupport]=printsupport; INFIX[QtPrintSupport]=PRINTSUPPORT

# Qt version, taken from the qtbase sources (QT_REPO_MODULE_VERSION in .cmake.conf)
QT_VERSION=$(sed -n 's/^set(QT_REPO_MODULE_VERSION "\{0,1\}\([0-9.]*\)".*/\1/p' "$QTSRC/.cmake.conf" | head -1)
[ -n "$QT_VERSION" ] || { echo "cannot determine Qt version of $QTSRC" >&2; exit 1; }
QT_MAJOR=${QT_VERSION%%.*}
rest=${QT_VERSION#*.}
QT_MINOR=${rest%%.*}
QT_PATCH=${rest#*.}
QT_VERSION_HEX=$(printf '0x%02x%02x%02x' "$QT_MAJOR" "$QT_MINOR" "$QT_PATCH")
QT_COPYRIGHT='Copyright (C) The Qt Company Ltd. and other contributors.'

# Features that a stock desktop Linux Qt build does not have.  Everything else
# declared in the qtbase configure.cmake files is reported as enabled.
EXCLUDE_FEATURES='^(windows|windows_*|winrt|winsdkicu|cpp_winrt|win32|directwrite|directwritecolrv1|sspi|metal|vsp2|android*|qnnx*|qqnx*|wasm*|emscripten*|webos*|vxworks*|integrity*|haiku*|macos*|ios*|tvos*|watchos*|uikit*|darwin*|appstore*|framework|simulator_and_device|slog2|harmony*|ohos*|batch_test_support|testlib_selfcover|valgrind|journald|lttng|etw|syslog|jemalloc|system_jpeg|opengles2|opengles3|opengles31|opengles32|gssapi|brotli|static|debug|debug_and_release|force_debug_info|separate_debug_info|force_asserts|optimize_.*|sanitize.*|sanitizer|use_.*_linker|gc_binaries|msvc.*|plugin_manifests|lint_generated_code|doc_snippets|no_prefix|relocatable|elf_private_full_version|pgo|ltcg|developer_build|private_tests|cross_compile|cxx2[0-9]_.*|separate_debug_info|test_gui)$'

rm -rf "$OUT"
for entry in $MODULES; do
    qm=${entry#*:}
    mkdir -p "$OUT/$qm/private"
done
mkdir -p "$OUT/QtGui/qpa"

# --------------------------------------------------------------------------
# 1. forwarding headers for every public/private header of every module
# --------------------------------------------------------------------------
for entry in $MODULES; do
    srcdir=${entry%%:*}; qm=${entry#*:}
    while IFS= read -r -d '' f; do
        base=$(basename "$f")
        if [[ "$base" == *_p.h ]]; then
            dest="$OUT/$qm/private/$base"
        else
            dest="$OUT/$qm/$base"
        fi
        [ -e "$dest" ] && { echo "warning: duplicate $base, keeping first" >&2; continue; }
        ln -s "$(realpath --relative-to="$(dirname "$dest")" "$f")" "$dest"
    done < <(find "$QTSRC/src/$srcdir" -name '*.h' \
                  -not -path '*/3rdparty/*' -not -path '*/doc/*' -print0)
done

# QPA headers live under QtGui/qpa in an installed Qt
while IFS= read -r -d '' f; do
    base=$(basename "$f")
    dest="$OUT/QtGui/qpa/$base"
    [ -e "$dest" ] && continue
    ln -s "$(realpath --relative-to="$(dirname "$dest")" "$f")" "$dest"
done < <(find "$QTSRC/src/gui/platform" -name '*.h' -print0)
while IFS= read -r -d '' f; do
    base=$(basename "$f")
    dest="$OUT/QtGui/qpa/$base"
    [ -e "$dest" ] && continue
    ln -s "$(realpath --relative-to="$(dirname "$dest")" "$f")" "$dest"
done < <(find "$QTSRC/src/gui" -name 'qplatform*.h' -not -path '*/3rdparty/*' -print0)

# --------------------------------------------------------------------------
# 2. class include shorthands: <QtModule>/QClassName
#    first pass  - QFoo declared in qfoo.h
#    second pass - any other exported class declared in a public header
# --------------------------------------------------------------------------
pragma_scan() {
    # The authoritative answer: every public Qt header declares which classes
    # it is the include shorthand for with
    #
    #     #pragma qt_class(QFoo)
    #
    # syncqt (Qt's own header generator) uses exactly this pragma, and it is
    # far more reliable than searching for "class QFoo" -- the declaration may
    # carry macros around it, e.g.
    # "class Q_CORE_EXPORT QTextStream QT_TEXT_STREAM_FINAL : public ...".
    grep -oE '^#pragma[[:space:]]+qt_class[ ]*[(][ ]*[A-Za-z0-9_]+[ ]*[)]' "$1" 2>/dev/null \
        | grep -oE '[(][ ]*[A-Za-z0-9_]+[ ]*[)]' \
        | tr -d '() ' \
        | sort -u || true
}

class_scan() {
    # Exported (or template) classes that are *defined* in a header -- a
    # forward declaration ("class QFoo;") ends with ';' and is skipped, so
    # that <QtModule>/QFoo cannot end up pointing at a header that merely
    # mentions the class.  The export macro in front of the class name is
    # optional and may itself be wrapped, e.g. "class QT6_ONLY(Q_CORE_EXPORT)
    # QDebug : public QIODeviceBase".
    grep -E '(class|struct)[[:space:]]+([A-Za-z_][A-Za-z0-9_]*(\([^)]*\))?[[:space:]]+)*Q[A-Z][A-Za-z0-9_]*([[:space:]]*(:|\{)|[[:space:]]*$)' \
        "$1" 2>/dev/null \
        | grep -vE ';' \
        | grep -oE 'Q[A-Z][A-Za-z0-9_]*([[:space:]]*(:|\{)|[[:space:]]*$)' \
        | grep -oE '^Q[A-Z][A-Za-z0-9_]*' \
        | sort -u || true
}

for entry in $MODULES; do
    qm=${entry#*:}
    # zeroth pass: the #pragma qt_class() marker of every public header
    for hdr in $(ls "$OUT/$qm"); do
        [ -f "$OUT/$qm/$hdr" ] || continue
        for cls in $(pragma_scan "$OUT/$qm/$hdr" || true); do
            [ -e "$OUT/$qm/$cls" ] && continue
            echo "#include <$qm/$hdr>" > "$OUT/$qm/$cls"
        done
    done
    # first pass: qfoo.h declares QFoo (compared case insensitively, so that
    # qpushbutton.h finds QPushButton and qopenglwidget.h finds QOpenGLWidget)
    for hdr in $(ls "$OUT/$qm"); do
        [ -f "$OUT/$qm/$hdr" ] || continue
        stem=${hdr%.h}
        cls=$(class_scan "$OUT/$qm/$hdr" | grep -ix "$stem" | head -1 || true)
        [ -n "$cls" ] && echo "#include <$qm/$hdr>" > "$OUT/$qm/$cls"
    done
    # second pass: every other class defined in a public header, as long as no
    # shorthand exists yet
    for hdr in $(ls "$OUT/$qm"); do
        [ -f "$OUT/$qm/$hdr" ] || continue
        for cls in $(class_scan "$OUT/$qm/$hdr" || true); do
            [ -e "$OUT/$qm/$cls" ] && continue
            echo "#include <$qm/$hdr>" > "$OUT/$qm/$cls"
        done
done

    # third pass: a shorthand for every lower case header that does not have one
    # yet.  class_scan() only finds "class QFoo" declarations, so the type
    # aliases Qt 6 added for source compatibility (QVector, QLinkedList, ...)
    # and the template heavy headers need this fallback: qvector.h defines
    # "using QVector = QList<T>", not "class QVector".
    for hdr in $(ls "$OUT/$qm"); do
        [ -f "$OUT/$qm/$hdr" ] || continue
        case "$hdr" in
            q*.h) ;;
            *) continue ;;
        esac
        stem=${hdr#q}
        stem=${stem%.h}
        first=$(echo "$stem" | cut -c1 | tr '[:lower:]' '[:upper:]')
        cls="Q${first}${stem#?}"
        [ -e "$OUT/$qm/$cls" ] && continue
        echo "#include <$qm/$hdr>" > "$OUT/$qm/$cls"
    done
done

# 3. generated export headers (<QtModule>/qt<module>exports.h)
# --------------------------------------------------------------------------
emit_exports() {
    local qm=$1 base=$2 infix=$3 file=$4 guard
    guard=$(echo "${base}_H" | tr '[:lower:]' '[:upper:]')
    cat > "$file" <<EOF
// Generated by scripts/toolchain/generate-qt-headers.sh -- do not edit.
#ifndef $guard
#define $guard

#include <QtCore/qcompilerdetection.h>
#include <QtCore/qtconfigmacros.h>

#if defined(QT_SHARED) || !defined(QT_STATIC)
#  if defined(QT_BUILD_${infix}_LIB)
#    define Q_${infix}_EXPORT Q_DECL_EXPORT
#  else
#    define Q_${infix}_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define Q_${infix}_EXPORT
#endif

#if !defined(QT_BUILD_${infix}_LIB) && !defined(QT_STATIC)
#  define QT_${infix}_INLINE_SINCE(major, minor) inline
#  define QT_${infix}_CONSTEXPR_INLINE_SINCE(major, minor) constexpr
#  define QT_${infix}_INLINE_IMPL_SINCE(major, minor) 1
#  define QT_${infix}_REMOVED_SINCE(major, minor) 0
#else
#  define QT_${infix}_INLINE_SINCE(major, minor) inline
#  define QT_${infix}_CONSTEXPR_INLINE_SINCE(major, minor) constexpr
#  define QT_${infix}_INLINE_IMPL_SINCE(major, minor) 1
#  define QT_${infix}_REMOVED_SINCE(major, minor) 0
#endif

#endif // $guard
EOF
}

for entry in $MODULES; do
    qm=${entry#*:}; srcdir=${entry%%:*}
    lower=$(echo "$qm" | sed 's/^Qt//' | tr '[:upper:]' '[:lower:]')
    base="qt${lower}exports"
    case "$qm" in
        QtTest) base=qttestexports ;;
        QtPrintSupport) base=qtprintsupportexports ;;
    esac
    emit_exports "$qm" "$base" "${INFIX[$qm]}" "$OUT/$qm/$base.h"
done

# --------------------------------------------------------------------------
# 4. generated feature headers
# --------------------------------------------------------------------------
features_from() {
    # every qt_feature("<name>") and qt_feature_deprecated("<name>") of a
    # configure.cmake, with the name normalised the way
    # qt_feature_normalize_name() does it (outside [a-zA-Z0-9_] -> '_')
    grep -oE '^qt_feature(_deprecated)?[(]"[A-Za-z0-9+._-]+"' "$1" 2>/dev/null \
        | sed -e 's/^qt_feature\(_deprecated\)\{0,1\}[(]"//' -e 's/"$//' \
              -e 's/[^a-zA-Z0-9_]/_/g' \
        | sort -u || true
}

emit_feature_header() {
    local file=$1 guard=$2 extra=$3; shift 3
    local features="$*"
    {
        echo "// Generated by scripts/toolchain/generate-qt-headers.sh -- do not edit."
        echo "#ifndef $guard"
        echo "#define $guard"
        echo
        for f in $features; do
            if echo "$f" | grep -qE "$EXCLUDE_FEATURES"; then
                echo "#define QT_FEATURE_$f -1"
            else
                echo "#define QT_FEATURE_$f 1"
            fi
        done
        if [ -n "$extra" ]; then
            echo
            echo "$extra"
        fi
        echo
        echo "#endif // $guard"
    } > "$file"
}

# global features -> QtCore/qconfig.h (+ private/qconfig_p.h)
global_features=$(features_from "$QTSRC/configure.cmake")

# qt_extra_definition() entries of the global configure.cmake: the version
# macros every Qt header expects to find in <QtCore/qconfig.h>.
global_extra="#define QT_VERSION_STR \"$QT_VERSION\"
#define QT_VERSION_MAJOR $QT_MAJOR
#define QT_VERSION_MINOR $QT_MINOR
#define QT_VERSION_PATCH $QT_PATCH
#define QT_COPYRIGHT \"$QT_COPYRIGHT\""

emit_feature_header "$OUT/QtCore/qconfig.h" QT_FEATURES_GLOBAL_QCONFIG_H "$global_extra" $global_features
emit_feature_header "$OUT/QtCore/private/qconfig_p.h" QT_FEATURES_GLOBAL_QCONFIG_P_H "" $global_features

# per module features -> <QtModule>/qt<module>-config.h (+ private twin)
for entry in $MODULES; do
    qm=${entry#*:}; srcdir=${entry%%:*}
    cfg=${CFGMOD[$qm]:-}
    [ -n "$cfg" ] || continue
    file="$QTSRC/src/$srcdir/configure.cmake"
    [ -f "$file" ] || continue
    feats=$(features_from "$file")
    [ -n "$feats" ] || continue
    hdr="qt${cfg}-config"
    emit_feature_header "$OUT/$qm/$hdr.h" "QT_FEATURES_$(echo "$cfg" | tr '[:lower:]' '[:upper:]')_CONFIG_H" "" $feats
    emit_feature_header "$OUT/$qm/private/${hdr}_p.h" "QT_FEATURES_$(echo "$cfg" | tr '[:lower:]' '[:upper:]')_CONFIG_P_H" "" $feats
done

# --------------------------------------------------------------------------
# 5. <QtCore>/qtdeprecationdefinitions.h (normally configured from the .h.in)
# --------------------------------------------------------------------------
cat > "$OUT/QtCore/qtdeprecationdefinitions.h" <<EOF
// Generated by scripts/toolchain/generate-qt-headers.sh -- do not edit.
#ifndef QTDEPRECATIONDEFINITIONS_H
#define QTDEPRECATIONDEFINITIONS_H

#include <QtCore/qcompilerdetection.h>

#ifndef QT_DISABLE_DEPRECATED_UP_TO
#  ifdef QT_DISABLE_DEPRECATED_BEFORE
#    define QT_DISABLE_DEPRECATED_UP_TO QT_DISABLE_DEPRECATED_BEFORE
#  else
#    define QT_DISABLE_DEPRECATED_UP_TO $QT_VERSION_HEX
#  endif
#endif

#ifndef QT_WARN_DEPRECATED_UP_TO
#  ifdef QT_DEPRECATED_WARNINGS_SINCE
#    define QT_WARN_DEPRECATED_UP_TO QT_DEPRECATED_WARNINGS_SINCE
#  else
#    define QT_WARN_DEPRECATED_UP_TO $QT_VERSION_HEX
#  endif
#endif

// Some Qt Core headers (<QtCore/qlogging.h> among them) use
// QT_DEPRECATED_SINCE() before qglobal.h gets to <QtCore/qtdeprecationmarkers.h>.
// The macros are therefore defined here as well, using exactly the expansion
// that qtdeprecationmarkers.h uses, so its later definition is an identical
// (and therefore warning free) redefinition.
#if defined(QT_NO_DEPRECATED)
#  define QT_DEPRECATED_SINCE(major, minor) 0
#else
#  if !defined(QT_NO_DEPRECATED_WARNINGS)
#    define QT_DEPRECATED Q_DECL_DEPRECATED
#    define QT_DEPRECATED_X(text) Q_DECL_DEPRECATED_X(text)
#    define QT_DEPRECATED_VARIABLE Q_DECL_VARIABLE_DEPRECATED
#    define QT_DEPRECATED_CONSTRUCTOR Q_DECL_CONSTRUCTOR_DEPRECATED explicit
#    define QT_DEPRECATED_MACRO(macro, text) Q_DECL_DEPRECATED_MACRO(macro, text)
#  endif
#  ifdef QT_DEPRECATED
#    define QT_DEPRECATED_SINCE(major, minor) (QT_VERSION_CHECK(major, minor, 0) > QT_DISABLE_DEPRECATED_UP_TO)
#  else
#    define QT_DEPRECATED_SINCE(major, minor) 0
#  endif
#endif

#endif // QTDEPRECATIONDEFINITIONS_H
EOF

# --------------------------------------------------------------------------
# 6. module umbrella headers (<QtModule>/<QtModule>)
# --------------------------------------------------------------------------
for entry in $MODULES; do
    qm=${entry#*:}
    {
        echo "// Generated by scripts/toolchain/generate-qt-headers.sh -- do not edit."
        echo "#ifndef QT_MODULE_$(echo "$qm" | tr '[:lower:]' '[:upper:]')_H"
        echo "#define QT_MODULE_$(echo "$qm" | tr '[:lower:]' '[:upper:]')_H"
        for hdr in $(ls "$OUT/$qm" | grep '^[a-z].*\.h$' | grep -v 'config\.h$'); do
            echo "#include <$qm/$hdr>"
        done
        echo "#endif"
    } > "$OUT/$qm/$qm"
done

echo "Qt headers generated in $OUT"
