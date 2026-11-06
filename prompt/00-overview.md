WriteCam is a Qt Quick and C++ Markdown writer forked from Omawrite, with YAML front matter and date-stamped filenames. These prompts make it a clean fork that tracks omacom/omawrite, then bring Write_On's alternative control, ghosting, Overflow and Lab across, one small thing per file.

House style is the existing Omawrite C++ and QML style; qmllint is the lint gate, and the last line of every file means `./bin/test && ./bin/lint` run clean. Set that command as `CHECK_CMD` when you run the pack.

Each file is the whole instruction for its session. Read only the one you are running.
