import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:async/async.dart';
import 'package:ffi/ffi.dart';

const _kFFToolsMessageTypeReturnCode = 0;
const _kFFToolsMessageTypeLog = 1;
const _kFFToolsMessageTypeStatistics = 2;

sealed class FFToolsMessage extends Struct {
  @Int32()
  external int type;
  external FFToolsMessageUnion data;
}

sealed class FFToolsMessageUnion extends Union {
  @Int32()
  external int returnCode;
  external FFToolsMessageLog log;
  external FFToolsMessageStatistics statistics;
}

sealed class FFToolsMessageLog extends Struct {
  @Int32()
  external int level;
  external Pointer<Utf8> message;
}

sealed class FFToolsMessageStatistics extends Struct {
  @Int32()
  external int frameNumber;
  @Float()
  external double fps;
  @Float()
  external double quality;
  @Int64()
  external int size;
  @Int32()
  external int time;
  @Double()
  external double bitrate;
  @Double()
  external double speed;
}

Future<(int, String)> ffprobe(List<String> args) {
  final completer = Completer<(int, String)>();
  final fftools = DynamicLibrary.open('../build/libfftools-ffi.dylib');
	final initialize = fftools.lookupFunction<Void Function(Pointer), void Function(Pointer)>('FFToolsFFIInitialize');
	initialize(NativeApi.postCObject);
	final ffprobe = fftools.lookupFunction<Void Function(Int64, Int, Pointer<Pointer<Utf8>>), void Function(int, int, Pointer<Pointer<Utf8>>)>('FFToolsFFIExecuteFFprobe');
  // Not freeing this memory is intentional. FFToolsFFI will free it when done execution.
  final argv = malloc<Pointer<Utf8>>(args.length);
  for (int i = 0; i < args.length; i++) {
    argv[i] = args[i].toNativeUtf8(allocator: malloc);
  }
  final port = ReceivePort();
  const logLevel = 32;
  final buffer = StringBuffer();
  port.listen((data) {
    if (data is int) {
      final messagePointer = Pointer<FFToolsMessage>.fromAddress(data);
      switch (messagePointer.ref.type) {
        case _kFFToolsMessageTypeReturnCode:
          port.close();
          completer.complete((messagePointer.ref.data.returnCode, buffer.toString()));
        case _kFFToolsMessageTypeLog:
          if (messagePointer.ref.data.log.level <= logLevel) {
            buffer.write(messagePointer.ref.data.log.message.toDartString());
          }
        case _kFFToolsMessageTypeStatistics:
          final stats = messagePointer.ref.data.statistics;
          print('Statistics frameNumber: ${stats.frameNumber}, fps: ${stats.fps}, quality: ${stats.quality}, size: ${stats.size}, time: ${stats.time}, bitrate: ${stats.bitrate}, speed: ${stats.speed}');
      }
      if (messagePointer.ref.type == _kFFToolsMessageTypeLog) {
        malloc.free(messagePointer.ref.data.log.message);
      }
      malloc.free(messagePointer);
    }
  });
  ffprobe(port.sendPort.nativePort, args.length, argv);
  return completer.future;
}

CancelableOperation<(int, String)> ffmpeg(List<String> args) {
  final fftools = DynamicLibrary.open('../build/libfftools-ffi.dylib');
	final initialize = fftools.lookupFunction<Void Function(Pointer), void Function(Pointer)>('FFToolsFFIInitialize');
  initialize(NativeApi.postCObject);
  final ffmpeg = fftools.lookupFunction<Void Function(Int64, Int, Pointer<Pointer<Utf8>>), void Function(int, int, Pointer<Pointer<Utf8>>)>('FFToolsFFIExecuteFFmpeg');
  final cancel = fftools.lookupFunction<Void Function(Int64), void Function(int)>('FFToolsCancel');
  final port = ReceivePort();
  final completer = CancelableCompleter<(int, String)>(
    onCancel: () {
      cancel(port.sendPort.nativePort);
    }
  );
  // Not freeing this memory is intentional. FFToolsFFI will free it when done execution.
  final argv = malloc<Pointer<Utf8>>(args.length);
  for (int i = 0; i < args.length; i++) {
    argv[i] = args[i].toNativeUtf8(allocator: malloc);
  }
  const logLevel = 32;
  final buffer = StringBuffer();
  port.listen((data) {
    if (data is int) {
      final messagePointer = Pointer<FFToolsMessage>.fromAddress(data);
      switch (messagePointer.ref.type) {
        case _kFFToolsMessageTypeReturnCode:
          port.close();
          completer.complete((messagePointer.ref.data.returnCode, buffer.toString()));
        case _kFFToolsMessageTypeLog:
          if (messagePointer.ref.data.log.level <= logLevel) {
            stdout.write(messagePointer.ref.data.log.message.toDartString());
            buffer.write(messagePointer.ref.data.log.message.toDartString());
          }
        case _kFFToolsMessageTypeStatistics:
          final stats = messagePointer.ref.data.statistics;
          print('Statistics frameNumber: ${stats.frameNumber}, fps: ${stats.fps}, quality: ${stats.quality}, size: ${stats.size}, time: ${stats.time}, bitrate: ${stats.bitrate}, speed: ${stats.speed}');
      }
      if (messagePointer.ref.type == _kFFToolsMessageTypeLog) {
        malloc.free(messagePointer.ref.data.log.message);
      }
      malloc.free(messagePointer);
    }
  });
  print('Executing ffmpeg');
  ffmpeg(port.sendPort.nativePort, args.length, argv);
  print('Returning operation');
  return completer.operation;
}


void main() async {
	final operation = ffmpeg([
    '-i',
    'https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_30MB.mp4',
    'bbb.webm'
  ]).then((r) {
    print('Callum result');
    print(r);
  });
  await Future.delayed(const Duration(milliseconds: 500));
  operation.cancel();
}
