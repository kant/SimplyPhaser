// Copyright © 2022 Brad Howes. All rights reserved.

import AUv3Support
import CoreAudioKit
import Kernel
import KernelBridge
import Knob_macOS
import ParameterAddress
import Parameters
import os.log

extension Knob: AUParameterValueProvider, RangedControl {}

/**
 Controller for the AUv3 filter view. Handles wiring up of the controls with AUParameter settings.
 */
@objc open class ViewController: AUViewController {

  // NOTE: this special form sets the subsystem name and must run before any other logger calls.
  private let log: OSLog = Shared.logger(Bundle.main.auBaseName + "AU", "ViewController")

  private let parameters = AudioUnitParameters()
  private var viewConfig: AUAudioUnitViewConfiguration!
  // private var parameterObserverToken: AUParameterObserverToken?
  private var keyValueObserverToken: NSKeyValueObservation?
  private var hasActiveLabel = false

  @IBOutlet private weak var controlsView: NSView!

  @IBOutlet private weak var rateControl: Knob!
  @IBOutlet private weak var rateValueLabel: FocusAwareTextField!

  @IBOutlet private weak var depthControl: Knob!
  @IBOutlet private weak var depthValueLabel: FocusAwareTextField!

  @IBOutlet private weak var intensityControl: Knob!
  @IBOutlet private weak var intensityValueLabel: FocusAwareTextField!

  @IBOutlet private weak var wetMixControl: Knob!
  @IBOutlet private weak var wetMixValueLabel: FocusAwareTextField!

  @IBOutlet private weak var dryMixControl: Knob!
  @IBOutlet private weak var dryMixValueLabel: FocusAwareTextField!

  @IBOutlet private weak var odd90Control: NSSwitch!

  private lazy var controls: [ParameterAddress: (Knob, Label)] = [
    .rate: (rateControl, rateValueLabel),
    .depth: (depthControl, depthValueLabel),
    .intensity: (intensityControl, intensityValueLabel),
    .dry: (dryMixControl, dryMixValueLabel),
    .wet: (wetMixControl, wetMixValueLabel)
  ]

  var editors = [ParameterAddress : AUParameterEditor]()
  public var audioUnit: FilterAudioUnit? {
    didSet {
      DispatchQueue.main.async {
        if self.isViewLoaded {
          self.createEditors()
        }
      }
    }
  }
}

public extension ViewController {

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = .black

    if audioUnit != nil {
      createEditors()
    }
  }

  override func mouseDown(with event: NSEvent) {
    // Allow for clicks on the common NSView to end editing of values
    NSApp.keyWindow?.makeFirstResponder(nil)

    os_log(.debug, log: log, "controlChanged END")
  }
}

// MARK: - AudioUnitViewConfigurationManager

extension ViewController: AudioUnitViewConfigurationManager {}

// MARK: - AUAudioUnitFactory

extension ViewController: AUAudioUnitFactory {
  @objc public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
    let audioUnit = try FilterAudioUnitFactory.create(componentDescription: componentDescription,
                                                      parameters: parameters,
                                                      kernel: KernelBridge(Bundle.main.auBaseName),
                                                      viewConfigurationManager: self)
    self.audioUnit = audioUnit
    return audioUnit
  }
}

// MARK: - Private

private extension ViewController {

  func createEditors() {
    let knobColor = NSColor(named: "knob")!
    odd90Control.setTint(knobColor)

    for (parameterAddress, (knob, label)) in controls {
      knob.progressColor = knobColor
      knob.indicatorColor = knobColor

      knob.target = self
      knob.action = #selector(handleKnobValueChange(_:))

      let trackWidth: CGFloat = parameterAddress == .dry || parameterAddress == .wet ? 8 : 10
      let progressWidth = trackWidth - 2.0
      knob.trackLineWidth = trackWidth
      knob.progressLineWidth = progressWidth
      knob.indicatorLineWidth = progressWidth

      let editor = FloatParameterEditor(parameter: parameters[parameterAddress],
                                        formatter: parameters.valueFormatter(parameterAddress),
                                        rangedControl: knob, label: label)
      editors[parameterAddress] = editor
    }

    editors[.odd90] = BooleanParameterEditor(parameter: parameters[.odd90], booleanControl: odd90Control)
  }

  @IBAction func handleKnobValueChange(_ control: Knob) {
    guard let address = control.parameterAddress else { fatalError() }
    controlChanged(control, address: address)
  }

  @IBAction func handleOdd90Change(_ control: NSSwitch) {
    controlChanged(control, address: .odd90)
  }

  func controlChanged(_ control: AUParameterValueProvider, address: ParameterAddress) {
    os_log(.info, log: log, "controlChanged BEGIN - %d %f", address.rawValue, control.value)

    guard let audioUnit = audioUnit else {
      os_log(.info, log: log, "controlChanged END - nil audioUnit")
      return
    }

    // When user changes something and a factory preset was active, clear it.
    audioUnit.clearCurrentPresetIfFactoryPreset()

    editors[address]?.controlChanged(source: control)
  }
}
