//
//  ContentView.swift
//  AirPurifier
//
//  Created by Aliou Tippett on 4/25/26.
//

import SwiftUI

struct ContentView: View {
    @StateObject var viewModel = AirPurifierViewModel()
    @State private var showDeviceList: Bool = false

    var body: some View {
        NavigationView {
            VStack(spacing: 24) {

                Spacer()

                // App Icon / Header
                VStack(spacing: 12) {
                    Image(systemName: "wind")
                        .font(.system(size: 80))
                        .foregroundColor(.blue)

                    Text("Smart Air Purifier")
                        .font(.largeTitle)
                        .fontWeight(.bold)

                    Text("Connect to your device to get started")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .multilineTextAlignment(.center)
                }

                Spacer()

                // Connection Status
                HStack(spacing: 10) {
                    Circle()
                        .fill(viewModel.isConnected ? Color.green : Color.red)
                        .frame(width: 12, height: 12)
                    Text(viewModel.isConnected ? "Connected" : "Disconnected")
                        .font(.headline)
                    Spacer()
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)

                // Error Message
                if let error = viewModel.errorMessage {
                    Text(error)
                        .foregroundColor(.red)
                        .font(.subheadline)
                        .padding()
                        .background(Color(.secondarySystemBackground))
                        .cornerRadius(12)
                }

                // Connect Button
                Button {
                    if viewModel.isConnected {
                        viewModel.disconnect()
                    } else {
                        showDeviceList = true
                    }
                } label: {
                    Text(viewModel.isConnected ? "Disconnect" : "Connect to Device")
                        .font(.headline)
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(viewModel.isConnected ? Color.red : Color.blue)
                        .foregroundColor(.white)
                        .cornerRadius(12)
                }

                // Hidden navigation link to ControlPanelView
                NavigationLink(
                    destination: ControlPanelView(viewModel: viewModel),
                    isActive: $viewModel.isConnected
                ) {
                    EmptyView()
                }

                Spacer()
            }
            .padding()
            .navigationBarHidden(true)
            .sheet(isPresented: $showDeviceList) {
                DeviceListView(viewModel: viewModel)
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}

#Preview {
    ContentView()
}
