# fftools-ffi
FFMPEG's fftools (`ffmpeg`, `ffprobe`) rearranged as a thread-safe library for use in (Dart) FFI

Notes
1. Replace the default config.h with the one when you built ffmpeg to get all the metadata to match.
2. The design is for `ffmpeg_execute_with_callbacks` and `ffprobe_execute_with_callbacks` to be synchronous/blocking and issue their callbacks on the same thread.