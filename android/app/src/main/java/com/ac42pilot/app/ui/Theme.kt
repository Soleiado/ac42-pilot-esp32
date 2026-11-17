package com.ac42pilot.app.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

object Ac42Colors {
    val Background = Color(0xFF050D12)
    val BackgroundTop = Color(0xFF081821)
    val Panel = Color(0xFF0A1A23)
    val Panel2 = Color(0xFF0E222D)
    val Panel3 = Color(0xFF102A36)
    val Line = Color(0xFF15333F)
    val Line2 = Color(0xFF235566)
    val Text = Color(0xFFE9F7FB)
    val Muted = Color(0xFF6F8F9B)
    val Muted2 = Color(0xFF9AB0B8)
    val Cyan = Color(0xFF36D8E8)
    val CyanDim = Color(0xFF0C3D47)
    val Green = Color(0xFF36DD8A)
    val GreenDim = Color(0xFF0B3A25)
    val Red = Color(0xFFFF5A6B)
    val RedDim = Color(0xFF3D1118)
    val Amber = Color(0xFFFFB64A)
    val AmberDim = Color(0xFF3E2A0B)
    val Violet = Color(0xFFB08CFF)
}

private val colors = darkColorScheme(
    primary = Ac42Colors.Cyan,
    onPrimary = Color(0xFF021014),
    primaryContainer = Ac42Colors.CyanDim,
    onPrimaryContainer = Color(0xFFBFF8FF),
    secondary = Ac42Colors.Green,
    onSecondary = Color(0xFF032114),
    secondaryContainer = Ac42Colors.GreenDim,
    onSecondaryContainer = Color(0xFFB9FFDA),
    tertiary = Ac42Colors.Amber,
    onTertiary = Color(0xFF241600),
    background = Ac42Colors.Background,
    onBackground = Ac42Colors.Text,
    surface = Ac42Colors.Panel,
    onSurface = Ac42Colors.Text,
    surfaceVariant = Ac42Colors.Panel2,
    onSurfaceVariant = Ac42Colors.Muted2,
    outline = Ac42Colors.Line,
    error = Ac42Colors.Red,
    onError = Color(0xFF2A0508),
    errorContainer = Ac42Colors.RedDim,
    onErrorContainer = Color(0xFFFFB6BD),
)

private val typography = Typography().run {
    copy(
        headlineLarge = headlineLarge.copy(fontWeight = FontWeight.Bold, letterSpacing = (-1).sp),
        headlineMedium = headlineMedium.copy(fontWeight = FontWeight.Bold),
        titleLarge = titleLarge.copy(fontWeight = FontWeight.Bold, letterSpacing = 0.5.sp),
        titleMedium = titleMedium.copy(fontWeight = FontWeight.SemiBold),
        bodyLarge = bodyLarge.copy(fontFamily = FontFamily.SansSerif),
        bodyMedium = bodyMedium.copy(fontFamily = FontFamily.SansSerif),
        labelLarge = labelLarge.copy(fontWeight = FontWeight.Bold, letterSpacing = 0.8.sp),
        labelMedium = labelMedium.copy(fontWeight = FontWeight.Bold, letterSpacing = 0.7.sp),
        labelSmall = labelSmall.copy(fontWeight = FontWeight.Bold, letterSpacing = 1.sp),
    )
}

@Composable
fun Ac42PilotTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = colors,
        typography = typography,
        content = content,
    )
}

val NumericStyle: TextStyle = TextStyle(
    fontFeatureSettings = "tnum",
    fontWeight = FontWeight.SemiBold,
)
