package com.ac42pilot.app

import android.graphics.Color
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.ac42pilot.app.ui.Ac42PilotTheme
import com.ac42pilot.app.ui.Ac42ViewModel
import com.ac42pilot.app.ui.PilotScreen

class MainActivity : ComponentActivity() {
    private val viewModel: Ac42ViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
        )

        setContent {
            Ac42PilotTheme {
                val state by viewModel.state.collectAsStateWithLifecycle()
                val snackbarHostState = remember { SnackbarHostState() }

                LaunchedEffect(Unit) {
                    viewModel.events.collect { event ->
                        snackbarHostState.showSnackbar(event.message)
                    }
                }

                PilotScreen(
                    state = state,
                    snackbarHostState = snackbarHostState,
                    onMode = viewModel::setMode,
                    onCourse = viewModel::changeCourse,
                    onTack = viewModel::tack,
                    onResponse = viewModel::cycleResponse,
                    onStartNfu = viewModel::startNfu,
                    onStopNfu = viewModel::stopNfu,
                    onConnect = viewModel::connectTo,
                    onLoadSettings = viewModel::loadSettings,
                    onSaveSettings = viewModel::saveSettings,
                    onReboot = viewModel::rebootDevice,
                    onLoadLog = viewModel::loadLog,
                )
            }
        }
    }

    override fun onResume() {
        super.onResume()
        viewModel.startPolling()
    }

    override fun onPause() {
        viewModel.stopNfu()
        viewModel.stopPolling()
        super.onPause()
    }
}
