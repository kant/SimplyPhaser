// Copyright © 2021 Brad Howes. All rights reserved.

#pragma once

#import <AudioToolbox/AudioToolbox.h>

NS_ASSUME_NONNULL_BEGIN

/**
 Small Obj-C bridge between Swift and the C++ kernel classes. The `KernelBridge` package contains the actual
 adoption of the `AUParameterHandler` and `AudioRenderer` protocols. This is done because include files cannot see the
 Swift protocols.
 */
@interface KernelBridge : NSObject

- (nonnull id)init:(NSString*)appExtensionName;

@end

// These are the functions that satisfy the AudioRenderer protocol
@interface KernelBridge (AudioRenderer)

/**
 Configure the kernel for new format and max frame in preparation to begin rendering

 @param inputFormat the current format of the input bus
 @param maxFramesToRender the max frames to expect in a render request
 */
- (void)setRenderingFormat:(AVAudioFormat*)format maxFramesToRender:(AUAudioFrameCount)maxFramesToRender;

/**
 Rendering has stopped. Release any resources used to support it.
 */
- (void)renderingStopped;

/**
 Obtain a block to use for rendering with the kernel.

 @returns AUInternalRenderBlock instance
 */
- (AUInternalRenderBlock)internalRenderBlock; // :(nullable AUHostTransportStateBlock)transportStateBlock;

/**
 Set the bypass state.

 @param state new bypass value
 */
- (void)setBypass:(BOOL)state;

@end

// These are the functions that satisfy the AUParameterHandler protocol
@interface KernelBridge (AUParameterHandler)

- (void)set:(AUParameter *)parameter value:(AUValue)value;

- (AUValue)get:(AUParameter *)parameter;

@end

NS_ASSUME_NONNULL_END
