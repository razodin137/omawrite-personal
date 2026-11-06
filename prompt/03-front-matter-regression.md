Your job is to go in and prove WriteCam's front matter survived the merge.

Port the front-matter, updated-log and date-stamped filename tests onto the merged test suite, and make them pass without changing the editor's behaviour. A file that already has front matter keeps every field as written; only the updated log is maintained.

Done is when ./bin/test and ./bin/lint run clean
