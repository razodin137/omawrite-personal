Your job is to go in and teach WriteCam to keep alternatives under the text without leaving Markdown.

One plain Markdown file, no sidecar: the piece reads as it does now, and the alternatives, which version is showing and what is ghosted live in a trailing `<!-- writecam -->` comment. A file with none of that saves with no comment at all. Open a file edited elsewhere and keep the alternatives whose words are still there.

Done is when ./bin/test and ./bin/lint run clean
