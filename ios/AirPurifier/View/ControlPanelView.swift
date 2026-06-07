//
//  ControlPanelView.swift
//  AirPurifier
//
//  Created by Aliou Tippett on 4/25/26.
//

import SwiftUI

struct ControlPanelView: View {
    @ObservedObject var viewModel: AirPurifierViewModel
    @Environment(\.presentationMode) var presentationMode
    
    private var formattedDate: String {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: viewModel.purifier.lastUpdated)
    }
    
    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                
                // Error Message
                if let error = viewModel.errorMessage {
                    Text(error)
                        .foregroundColor(.red)
                        .font(.subheadline)
                        .padding()
                        .background(Color(.secondarySystemBackground))
                        .cornerRadius(12)
                }
                // Power Control
                VStack(spacing: 16) {
                    Text("Power")
                        .font(.headline)
                    Toggle(isOn: Binding(
                        get: { viewModel.purifier.isPowered },
                        set: { _ in viewModel.togglePower() }
                    )) {
                        Text(viewModel.purifier.isPowered ? "Purifier On" : "Purifier Off")
                    }
                    .toggleStyle(SwitchToggleStyle())
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
                
                // Fan Speed
                VStack(alignment: .leading, spacing: 16) {
                    Text("Fan Speed")
                        .font(.headline)
                    Picker(
                        "Fan Speed",
                        selection: Binding(
                            get: {
                                if viewModel.purifier.fanSpeed < 34 { return 0 }
                                if viewModel.purifier.fanSpeed < 67 { return 1 }
                                return 2
                            },
                            set: { selection in
                                switch selection {
                                case 0: viewModel.updateFanSpeed(25)
                                case 1: viewModel.updateFanSpeed(50)
                                default: viewModel.updateFanSpeed(100)
                                }
                            }
                        )
                    ) {
                        Text("Low").tag(0)
                        Text("Medium").tag(1)
                        Text("High").tag(2)
                    }
                    .pickerStyle(SegmentedPickerStyle())
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
                
                // Mode Picker
                VStack(alignment: .leading, spacing: 16) {
                    Text("Mode")
                        .font(.headline)
                    Picker("Purifier Mode", selection: Binding(
                        get: { viewModel.purifier.mode },
                        set: { viewModel.updateMode($0) }
                    )) {
                        ForEach(PurifierMode.allCases) { mode in
                            Text(mode.rawValue).tag(mode)
                        }
                    }
                    .pickerStyle(SegmentedPickerStyle())
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
                
                // Schedule
                VStack(alignment: .leading, spacing: 16) {
                    HStack {
                        Text("Schedule")
                            .font(.headline)
                        Spacer()
                        Text(viewModel.purifier.scheduleEnabled ? "Status: On" : "Status: Off")
                            .foregroundColor(.secondary)
                    }
                    Toggle("Enable Schedule", isOn: Binding(
                        get: { viewModel.purifier.scheduleEnabled },
                        set: { _ in viewModel.toggleSchedule() }
                    ))
                    HStack(spacing: 16) {
                        VStack(alignment: .leading, spacing: 8) {
                            Text("Start Time")
                                .font(.subheadline)
                            DatePicker(
                                "",
                                selection: Binding(
                                    get: { viewModel.purifier.scheduleStartTime },
                                    set: { viewModel.updateScheduleStart($0) }
                                ),
                                displayedComponents: .hourAndMinute
                            )
                            .labelsHidden()
                        }
                        .frame(maxWidth: .infinity)
                        
                        VStack(alignment: .leading, spacing: 8) {
                            Text("End Time")
                                .font(.subheadline)
                            DatePicker(
                                "",
                                selection: Binding(
                                    get: { viewModel.purifier.scheduleEndTime },
                                    set: { viewModel.updateScheduleEnd($0) }
                                ),
                                displayedComponents: .hourAndMinute
                            )
                            .labelsHidden()
                        }
                        .frame(maxWidth: .infinity)
                    }
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
                
                // Filter Status
                VStack(alignment: .leading, spacing: 12) {
                    Text("Filter Status")
                        .font(.headline)
                    HStack {
                        Text(viewModel.purifier.filterStatus)
                            .font(.body)
                        Spacer()
                    }
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
                
                // Last Updated
                VStack(alignment: .leading, spacing: 12) {
                    Text("Last Updated")
                        .font(.headline)
                    Text(formattedDate)
                        .foregroundColor(.secondary)
                }
                .padding()
                .background(Color(.secondarySystemBackground))
                .cornerRadius(12)
            }
            .padding()
        }
        .navigationTitle("Smart Air Purifier")
        .navigationBarTitleDisplayMode(.inline)
        .navigationBarBackButtonHidden(true)
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                Button("Disconnect") {
                    viewModel.disconnect()
                }
                .foregroundColor(.red)
            }
        }
        .onChange(of: viewModel.isConnected) { connected in
            if !connected {
                presentationMode.wrappedValue.dismiss()
            }
        }
    }
}
#Preview {
    ControlPanelView(viewModel: AirPurifierViewModel())
}
