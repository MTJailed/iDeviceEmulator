HXCOMM Use DEFHEADING() to define headings in both help text and texi
HXCOMM Text between STEXI and ETEXI are copied to texi version and
HXCOMM discarded from C version
HXCOMM DEF(command, callback, arg_string) is used to construct
HXCOMM command structures and help message.
HXCOMM HXCOMM can be used for comments, discarded from both texi and C

STEXI
@table @option
ETEXI

DEF("check", img_check,
    "check [-f fmt] filename")
STEXI
@item check [-f @var{fmt}] @var{filename}
ETEXI

DEF("create", img_create,
    "create [-f fmt] [-o options] filename [size]")
STEXI
@item create [-f @var{fmt}] [-o @var{options}] @var{filename} [@var{size}]
ETEXI

DEF("commit", img_commit,
    "commit [-f fmt] filename")
STEXI
@item commit [-f @var{fmt}] @var{filename}
ETEXI

DEF("convert", img_convert,
    "convert [-c] [-f fmt] [-O output_fmt] [-o options] [-s snapshot_name] filename [filename2 [...]] output_filename")
STEXI
@item convert [-c] [-f @var{fmt}] [-O @var{output_fmt}] [-o @var{options}] [-s @var{snapshot_name}] @var{filename} [@var{filename2} [...]] @var{output_filename}
ETEXI

DEF("info", img_info,
    "info [-f fmt] filename")
STEXI
@item info [-f @var{fmt}] @var{filename}
ETEXI

DEF("snapshot", img_snapshot,
    "snapshot [-l | -a snapshot | -c snapshot | -d snapshot] filename")
STEXI
@item snapshot [-l | -a @var{snapshot} | -c @var{snapshot} | -d @var{snapshot}] @var{filename}
ETEXI

DEF("rebase", img_rebase,
    "rebase [-f fmt] [-u] -b backing_file [-F backing_fmt] filename")
STEXI
@item rebase [-f @var{fmt}] [-u] -b @var{backing_file} [-F @var{backing_fmt}] @var{filename}
ETEXI

DEF("resize", img_resize,
    "resize filename [+ | -]size")
STEXI
@item resize @var{filename} [+ | -]@var{size}
@end table
ETEXI
