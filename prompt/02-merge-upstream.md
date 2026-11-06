Your job is to go in and bring the new Omawrite changes into WriteCam.

Merge omacom/omawrite master, through 0.7.2, into the WriteCam branch. Upstream wins everywhere except WriteCam's name and packaging, its front matter and its date-stamped filenames; keep those working. Resolve the conflicts in the editor, the highlighter and the tests.

Done is when ./bin/test and ./bin/lint run clean
