Your job is to go in and make this empty directory WriteCam, a fork of Omawrite.

Bring the WriteCam fork in from razodin137/omawrite-personal with its git history, keep `prompt/` and `scripts/`, set omacom/omawrite as upstream and the fork as origin, and keep WriteCam's name, packaging and `.scratch/` ignored. Add a `bin/lint` that runs qmllint over the QML.

Done is when ./bin/test and ./bin/lint run clean
