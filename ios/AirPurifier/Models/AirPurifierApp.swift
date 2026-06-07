//
//  AirPurifierApp.swift
//  AirPurifier
//
//  Created by Aliou Tippett on 4/25/26.
//

import SwiftUI

@main
struct AirPurifierApp: App {
    @StateObject private var viewModel = AirPurifierViewModel()
    
    var body: some Scene {
        WindowGroup {
            ContentView(viewModel: viewModel)
                .onAppear {
                  //  print("🔵 App appeared, BT state: \(viewModel.isConnected)")
                }
        }
    }
}
