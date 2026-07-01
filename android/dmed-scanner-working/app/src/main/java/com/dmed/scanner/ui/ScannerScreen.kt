package com.dmed.scanner.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dmed.scanner.data.DmedEndpoint

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ScannerScreen(viewModel: MainViewModel, onEndpointSelected: (DmedEndpoint) -> Unit) {
    val endpoints by viewModel.discovery.endpoints.collectAsState()
    val isScanning by viewModel.discovery.isScanning.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(title = { Text("📡 DMED BLE Scanner") })
        }
    ) { padding ->
        Column(
            modifier = Modifier.fillMaxSize().padding(padding).padding(16.dp)
        ) {
            Button(
                onClick = { if (isScanning) viewModel.stopScan() else viewModel.startScan() },
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(if (isScanning) "⏹ Stop Scanning" else "🔍 Scan for DMED Devices")
            }

            Spacer(modifier = Modifier.height(16.dp))

            if (isScanning && endpoints.isEmpty()) {
                Box(modifier = Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        CircularProgressIndicator()
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("Scanning for DMED BLE beacons...", style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }

            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(endpoints) { endpoint ->
                    EndpointCard(endpoint) { onEndpointSelected(endpoint) }
                }
            }
        }
    }
}

@Composable
fun EndpointCard(endpoint: DmedEndpoint, onClick: () -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth().clickable(onClick = onClick),
        elevation = CardDefaults.cardElevation(2.dp)
    ) {
        Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(endpoint.icon, fontSize = 32.sp)
            Spacer(modifier = Modifier.width(12.dp))
            Column {
                Text(endpoint.name, fontWeight = FontWeight.Bold)
                if (endpoint.description.isNotEmpty()) {
                    Text(endpoint.description, style = MaterialTheme.typography.bodySmall)
                }
                Text(endpoint.address,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.outline)
            }
        }
    }
}
