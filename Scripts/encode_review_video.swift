// macOS: swift -module-cache-path /tmp/lostexpedition-swift Scripts/encode_review_video.swift <frames> <output.mp4> <fps> <count>
// Encode every Unreal screenshot at its simulated timestep; never interpolate frames.
import Foundation
import AppKit
import AVFoundation

let args = CommandLine.arguments
 guard args.count == 5, let fps = Int32(args[3]), let count = Int(args[4]), fps > 0, count > 0 else {
    fatalError("Expected frame directory, new output path, fps and frame count")
}
let directory = URL(fileURLWithPath: args[1])
let output = URL(fileURLWithPath: args[2])
let width = 960, height = 600
let writer = try AVAssetWriter(outputURL: output, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: width, AVVideoHeightKey: height,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 2_000_000,
        AVVideoExpectedSourceFrameRateKey: fps, AVVideoMaxKeyFrameIntervalKey: fps]])
let attributes: [String: Any] = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB,
    kCVPixelBufferWidthKey as String: width, kCVPixelBufferHeightKey as String: height,
    kCVPixelBufferCGImageCompatibilityKey as String: true, kCVPixelBufferCGBitmapContextCompatibilityKey as String: true]
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: attributes)
writer.add(input)
guard writer.startWriting() else { fatalError("Cannot start writer: \(String(describing: writer.error))") }
writer.startSession(atSourceTime: .zero)
for frame in 0..<count {
    while !input.isReadyForMoreMediaData { Thread.sleep(forTimeInterval: 0.002) }
    try autoreleasepool {
        let path = directory.appendingPathComponent(String(format: "%04d.png", frame))
        guard let bitmap = NSBitmapImageRep(data: try Data(contentsOf: path)), let image = bitmap.cgImage else { fatalError("Missing frame \(frame)") }
        var buffer: CVPixelBuffer?
        guard CVPixelBufferPoolCreatePixelBuffer(nil, adaptor.pixelBufferPool!, &buffer) == kCVReturnSuccess, let buffer else { fatalError("Pixel buffer failed") }
        CVPixelBufferLockBaseAddress(buffer, [])
        let context = CGContext(data: CVPixelBufferGetBaseAddress(buffer), width: width, height: height,
            bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(buffer), space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue)!
        context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        CVPixelBufferUnlockBaseAddress(buffer, [])
        guard adaptor.append(buffer, withPresentationTime: CMTime(value: Int64(frame), timescale: fps)) else { fatalError("Encode failed: \(String(describing: writer.error))") }
    }
}
input.markAsFinished()
let completion = DispatchSemaphore(value: 0)
writer.finishWriting { completion.signal() }
completion.wait()
guard writer.status == .completed else { fatalError("Video failed: \(String(describing: writer.error))") }
print("Encoded \(count) frames at \(fps) fps: \(output.path)")
