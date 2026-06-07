//
//  AirPurifier.swift
//  AirPurifier
//
//  Created by Aliou Tippett on 4/25/26.
//

import Foundation

enum PurifierMode: String, CaseIterable, Identifiable {
    case auto = "Auto"
    case sleep = "Sleep"
    case turbo = "Turbo"

    var id: String { rawValue }
}

struct AirPurifier {
    var isPowered: Bool = false
    var fanSpeed: Int = 0
    var mode: PurifierMode = .auto
    var filterStatus: String = "Good"
    var lastUpdated: Date = Date()
    
    // Schedule
    var scheduleEnabled: Bool = false
    var scheduleStartTime: Date = Date()
    var scheduleEndTime: Date = Date()
}

