package com.ac42pilot.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun PilotScreen(
    state: UiState,
    snackbarHostState: SnackbarHostState,
    onMode: (String) -> Unit,
    onCourse: (String, Int) -> Unit,
    onTack: (String) -> Unit,
    onResponse: () -> Unit,
    onStartNfu: (String) -> Unit,
    onStopNfu: () -> Unit,
    onConnect: (String) -> Unit,
    onLoadSettings: () -> Unit,
    onSaveSettings: (SettingsForm) -> Unit,
    onReboot: () -> Unit,
    onLoadLog: () -> Unit,
) {
    var showSettings by rememberSaveable { mutableStateOf(false) }
    val pilotView = buildPilotView(state)
    val banner = bannerFor(state, pilotView)

    Scaffold(
        containerColor = Ac42Colors.Background,
        snackbarHost = { SnackbarHost(snackbarHostState) },
        contentWindowInsets = WindowInsets(0, 0, 0, 0),
    ) { padding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .windowInsetsPadding(WindowInsets.safeDrawing)
                .background(
                    Brush.verticalGradient(
                        listOf(Ac42Colors.BackgroundTop, Ac42Colors.Background),
                    ),
                ),
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 12.dp, vertical = 10.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                banner?.let { AlertBanner(it) }

                BoxWithConstraints(Modifier.fillMaxWidth()) {
                    val wide = maxWidth >= 900.dp
                    if (wide) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(12.dp),
                        ) {
                            GaugeCard(state, Modifier.weight(1.08f))
                            ControlsCard(
                                state = state,
                                modifier = Modifier.weight(0.92f),
                                onCourse = onCourse,
                                onTack = onTack,
                                onStartNfu = onStartNfu,
                                onStopNfu = onStopNfu,
                            )
                        }
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(12.dp),
                        ) {
                            PilotCard(
                                state = state,
                                modifier = Modifier.weight(1.08f),
                                onMode = onMode,
                                onResponse = onResponse,
                            )
                            InstrumentsCard(state, Modifier.weight(0.92f))
                        }
                    } else {
                        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                            GaugeCard(state, Modifier.fillMaxWidth())
                            ControlsCard(
                                state = state,
                                modifier = Modifier.fillMaxWidth(),
                                onCourse = onCourse,
                                onTack = onTack,
                                onStartNfu = onStartNfu,
                                onStopNfu = onStopNfu,
                            )
                            PilotCard(
                                state = state,
                                modifier = Modifier.fillMaxWidth(),
                                onMode = onMode,
                                onResponse = onResponse,
                            )
                            InstrumentsCard(state, Modifier.fillMaxWidth())
                        }
                    }
                }

                if (state.status?.simulated == true) {
                    BannerDemo()
                }

                Ac42Footer(state)

                Ac42TopBar(
                    state = state,
                    onSettings = {
                        showSettings = true
                        onLoadLog()
                    },
                )
            }
        }
    }

    if (showSettings) {
        SettingsDialog(
            state = state,
            onDismiss = { showSettings = false },
            onConnect = onConnect,
            onLoadSettings = onLoadSettings,
            onSaveSettings = onSaveSettings,
            onReboot = onReboot,
            onLoadLog = onLoadLog,
        )
    }
}

@Composable
private fun BannerDemo() {
    Surface(
        shape = RoundedCornerShape(8.dp),
        color = Ac42Colors.AmberDim,
        border = BorderStroke(1.dp, Color(0xFF684A13)),
    ) {
        Text(
            "MODE DÉMO",
            modifier = Modifier.padding(horizontal = 9.dp, vertical = 4.dp),
            color = Ac42Colors.Amber,
            fontSize = 9.sp,
            fontWeight = FontWeight.Black,
            letterSpacing = 1.5.sp,
        )
    }
}
