Your job is to go in and bring back WriteCam's auto-tagging on the merged tree.

It had a System One pass that read a tag schema, asked a local decision server for the tags of the piece, and merged them into the front matter. Bring it back opt-in, failing soft when the server is missing, and never holding up a save. Keep the tag schema editable.

Done is when ./bin/test and ./bin/lint run clean
