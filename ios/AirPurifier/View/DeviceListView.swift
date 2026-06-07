//
//  DeviceListView.swift
//  AirPurifier
//
//  Created by Aliou Tippett on 4/25/26.
//

import SwiftUI
import CoreBluetooth

struct DeviceListView: View {
    @ObservedObject var viewModel: AirPurifierViewModel
    @Environment(\.dismiss) var dismiss

    var body: some View {
        NavigationView {
            VStack(spacing: 0) {

                // Scanning indicator
                if viewModel.isScanning {
                    HStack(spacing: 12) {
                        ProgressView()
                        Text("Scanning for devices...")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                        Spacer()
                    }
                    .padding()
                    .background(Color(.secondarySystemBackground))
                }

                // Device list
                if viewModel.discoveredDevices.isEmpty {
                    Spacer()
                    VStack(spacing: 16) {
                        // Image(systemName: "bluetooth")
                        Image(systemName: "antenna.radiowaves.left.and.right") // safer SF image (more reconizable
                            .font(.system(size: 50))
                            .foregroundColor(.secondary)
                        Text(viewModel.isScanning ? "Looking for devices..." : "No devices found")
                            .font(.headline)
                            .foregroundColor(.secondary)
                        if !viewModel.isScanning {
                            Button("Scan Again") {
                                viewModel.connect()
                            }
                            .buttonStyle(.borderedProminent)
                        }
                    }
                    Spacer()
                } else {
                    List(viewModel.discoveredDevices, id: \.identifier) { peripheral in
                        Button {
                            viewModel.connectToDevice(peripheral)
                            dismiss()
                        } label: {
                            HStack {
                                //Image(systemName: "airpurifier")
                                Image(systemName: "wind") // safer more reconizable SF symbol
                                    .foregroundColor(.blue)
                                    .frame(width: 32)

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(peripheral.name ?? "Unknown Device")
                                        .font(.headline)
                                        .foregroundColor(.primary)
                                    Text(peripheral.identifier.uuidString)
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                        .lineLimit(1)
                                        .truncationMode(.middle)
                                }

                                Spacer()

                                Image(systemName: "chevron.right")
                                    .foregroundColor(.secondary)
                                    .font(.caption)
                            }
                            .padding(.vertical, 4)
                        }
                    }
                }
            }
            .navigationTitle("Select Device")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancel") {
                        viewModel.isScanning = false
                        dismiss()
                    }
                }
            }
        }
        .onAppear {
            viewModel.connect()
        }
    }
}
