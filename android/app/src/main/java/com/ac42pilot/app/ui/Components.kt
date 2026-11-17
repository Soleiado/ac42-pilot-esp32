package com.ac42pilot.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.ac42pilot.app.data.DeviceAddress
import com.ac42pilot.app.data.Pilot
import com.ac42pilot.app.data.ResponseState
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

// ---------------------------------------------------------------------------
// Basic building blocks
// ---------------------------------------------------------------------------

@Composable
fun Ac42Card(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(20.dp),
        color = Ac42Colors.Panel,
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
        tonalElevation = 0.dp,
        content = content,
    )
}

@Composable
fun BrandLogo(modifier: Modifier = Modifier) {
    Canvas(modifier.size(42.dp)) {
        val center = this.center
        val radius = size.minDimension * 0.46f
        drawCircle(Ac42Colors.Panel2, radius = radius, center = center)
        drawCircle(Ac42Colors.Line2, radius = radius, center = center, style = Stroke(1.5.dp.toPx()))
        rotate(-32f, center) {
            drawLine(
                color = Ac42Colors.Amber,
                start = center,
                end = Offset(center.x, center.y - radius * 0.72f),
                strokeWidth = 2.dp.toPx(),
                cap = StrokeCap.Round,
            )
        }
        drawLine(
            color = Ac42Colors.Cyan,
            start = center,
            end = Offset(center.x, center.y - radius * 0.62f),
            strokeWidth = 2.dp.toPx(),
            cap = StrokeCap.Round,
        )
        drawCircle(Ac42Colors.Text, radius = 2.dp.toPx(), center = center)
    }
}

@Composable
fun Ac42TopBar(
    state: UiState,
    onSettings: () -> Unit,
) {
    val connected = state.connected
    val wifi = state.status?.wifi
    val connectionText = when {
        !state.everConnected -> "Connexion au boîtier…"
        !connected -> "Liaison perdue"
        else -> {
            val ip = wifiAddress(state) ?: "boîtier"
            "$ip${if (wifi?.fallbackAp == true) " · AP secours" else ""} · ${state.status?.can?.state ?: "CAN"}"
        }
    }

    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val wide = maxWidth >= 680.dp
        if (wide) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                BrandBlock(connectionText, Modifier.weight(1f))
                StatusChips(state)
                SettingsGear(onSettings)
            }
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    BrandBlock(connectionText, Modifier.weight(1f))
                    SettingsGear(onSettings)
                }
                StatusChips(state)
            }
        }
    }
}

@Composable
private fun BrandBlock(subtitle: String, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        BrandLogo()
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.Bottom) {
                Text(
                    "AC42 ",
                    color = Ac42Colors.Text,
                    fontWeight = FontWeight.Black,
                    fontSize = 18.sp,
                    letterSpacing = 2.sp,
                    maxLines = 1,
                )
                Text(
                    "PILOT",
                    color = Ac42Colors.Cyan,
                    fontWeight = FontWeight.SemiBold,
                    fontSize = 18.sp,
                    letterSpacing = 2.sp,
                    maxLines = 1,
                )
            }
            Text(
                subtitle,
                color = Ac42Colors.Muted,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 0.8.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
private fun SettingsGear(onSettings: () -> Unit) {
    Surface(
        modifier = Modifier
            .size(38.dp)
            .clip(RoundedCornerShape(12.dp))
            .clickable(onClick = onSettings),
        shape = RoundedCornerShape(12.dp),
        color = Ac42Colors.Panel,
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
    ) {
        Box(contentAlignment = Alignment.Center) {
            Text("⚙", color = Ac42Colors.Muted2, fontSize = 19.sp)
        }
    }
}

@Composable
private fun StatusChips(state: UiState) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        StatusChip("CAN", canChipState(state))
        StatusChip("PUPITRE", controllerChipState(state))
        StatusChip("AC42", apChipState(state))
    }
}

@Composable
private fun StatusChip(label: String, state: String) {
    val color = when (state) {
        "ok" -> Ac42Colors.Green
        "warn" -> Ac42Colors.Amber
        "off" -> Ac42Colors.Red
        else -> Ac42Colors.Muted
    }
    val border = when (state) {
        "ok" -> Color(0xFF15583B)
        "warn" -> Color(0xFF5B4215)
        "off" -> Color(0xFF4B1A22)
        else -> Ac42Colors.Line
    }
    Surface(
        shape = RoundedCornerShape(50),
        color = Ac42Colors.Panel.copy(alpha = 0.8f),
        border = androidx.compose.foundation.BorderStroke(1.dp, border),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            Canvas(Modifier.size(7.dp)) {
                drawCircle(color)
            }
            Text(
                label,
                color = if (state == "off") Ac42Colors.Red else Ac42Colors.Muted2,
                fontSize = 8.sp,
                fontWeight = FontWeight.Black,
                letterSpacing = 0.7.sp,
            )
        }
    }
}

data class BannerInfo(val message: String, val error: Boolean = false, val info: Boolean = false)

fun bannerFor(state: UiState, view: PilotView): BannerInfo? {
    val status = state.status
    if (!state.connected) {
        return if (!state.everConnected && state.failures == 0) {
            BannerInfo("Connexion au boîtier…", info = true)
        } else {
            BannerInfo("Liaison avec le boîtier perdue. Vérifiez l’adresse IP dans les réglages.", error = true)
        }
    }
    if (status?.can?.ready != true) {
        return BannerInfo("Bus CAN non opérationnel. Vérifiez le câblage NMEA 2000 et la terminaison.", error = true)
    }
    if (status.controller.present != true) {
        return BannerInfo("Aucun pupitre/contrôleur B&G détecté. L’AC42 exige son heartbeat PGN 65305.", error = true)
    }
    if (!status.ap.known && !status.ap.forced) {
        return BannerInfo("Adresse de l’AC42 non détectée. Les commandes seront envoyées à 0x02.")
    }
    if (status.wifi.fallbackAp) {
        return BannerInfo("Réseau Wi-Fi configuré inaccessible — AP de secours actif.", info = true)
    }
    if (state.settingsRebootPending || status.config.rebootPending) {
        return BannerInfo("Réglages en attente de redémarrage. Ouvrez Réglages puis Redémarrer.", info = true)
    }
    if (status.pilot?.modeFresh != true) {
        return BannerInfo("État du pilote non reçu (PGN 65340/65305).")
    }
    if (view.heading == null) {
        return BannerInfo("Cap non reçu (PGN 127250).")
    }
    return null
}

@Composable
fun AlertBanner(banner: BannerInfo) {
    val border = when {
        banner.error -> Color(0xFF5D1C25)
        banner.info -> Color(0xFF174A57)
        else -> Color(0xFF614414)
    }
    val background = when {
        banner.error -> Ac42Colors.RedDim
        banner.info -> Ac42Colors.CyanDim
        else -> Ac42Colors.AmberDim
    }
    val textColor = when {
        banner.error -> Color(0xFFFFB0B8)
        banner.info -> Color(0xFFBFF8FF)
        else -> Color(0xFFFFD89C)
    }
    Surface(
        shape = RoundedCornerShape(12.dp),
        color = background,
        border = androidx.compose.foundation.BorderStroke(1.dp, border),
    ) {
        Text(
            banner.message,
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 9.dp),
            color = textColor,
            fontSize = 11.sp,
            fontWeight = FontWeight.SemiBold,
            lineHeight = 15.sp,
        )
    }
}

// ---------------------------------------------------------------------------
// Gauge
// ---------------------------------------------------------------------------

@Composable
fun GaugeCard(state: UiState, modifier: Modifier = Modifier) {
    val view = buildPilotView(state)
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(22.dp),
        color = Ac42Colors.Panel,
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
    ) {
        Column(
            modifier = Modifier.padding(10.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            CompassGauge(state, view, Modifier.fillMaxWidth())
            Row(
                modifier = Modifier.padding(top = 2.dp, bottom = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                LegendDot(Ac42Colors.Cyan, "Cap")
                LegendDot(Ac42Colors.Amber, "Cible")
                LegendDot(Ac42Colors.Violet, "Vent")
            }
        }
    }
}

@Composable
private fun LegendDot(color: Color, label: String) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Canvas(Modifier.size(7.dp)) { drawCircle(color) }
        Text(label, color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Bold, letterSpacing = 0.8.sp)
    }
}

@Composable
private fun CompassGauge(
    state: UiState,
    view: PilotView,
    modifier: Modifier = Modifier,
) {
    val textMeasurer = rememberTextMeasurer()
    val connected = state.connected
    val target = view.target
    BoxWithConstraints(modifier.aspectRatio(1f)) {
        val bigSp = (maxWidth.value * 0.185f).coerceIn(46f, 96f)
        Canvas(Modifier.fillMaxSize()) {
            val c = center
            val r = size.minDimension * 0.43f
            drawCircle(Ac42Colors.Panel3, radius = r * 1.05f, center = c, style = Stroke(width = size.minDimension * 0.055f))
            drawCircle(Ac42Colors.Line, radius = r, center = c, style = Stroke(width = 1.dp.toPx()))

            for (d in 0 until 360 step 5) {
                val major = d % 30 == 0
                val medium = d % 10 == 0
                val angle = Math.toRadians((d - 90).toDouble())
                val ux = cos(angle).toFloat()
                val uy = sin(angle).toFloat()
                val outer = Offset(c.x + ux * r, c.y + uy * r)
                val len = when {
                    major -> r * 0.095f
                    medium -> r * 0.06f
                    else -> r * 0.03f
                }
                val inner = Offset(c.x + ux * (r - len), c.y + uy * (r - len))
                drawLine(
                    color = when {
                        major -> Color(0xFF9FB6BF)
                        medium -> Color(0xFF4D6B77)
                        else -> Color(0xFF35535F)
                    },
                    start = outer,
                    end = inner,
                    strokeWidth = if (major) 2.2.dp.toPx() else 1.1.dp.toPx(),
                    cap = StrokeCap.Round,
                )
            }

            for (d in 0 until 360 step 30) {
                val cardinal = when (d) {
                    0 -> "N"
                    90 -> "E"
                    180 -> "S"
                    270 -> "W"
                    else -> d.toString()
                }
                val angle = Math.toRadians((d - 90).toDouble())
                val lr = r * 0.74f
                val pos = Offset(c.x + cos(angle).toFloat() * lr, c.y + sin(angle).toFloat() * lr)
                val layout = textMeasurer.measure(
                    text = cardinal,
                    style = TextStyle(
                        color = if (d % 90 == 0) Ac42Colors.Text else Ac42Colors.Muted2,
                        fontSize = if (d % 90 == 0) 22.sp else 12.sp,
                        fontWeight = if (d % 90 == 0) FontWeight.Bold else FontWeight.Medium,
                        textAlign = TextAlign.Center,
                    ),
                )
                drawText(
                    textLayoutResult = layout,
                    topLeft = Offset(pos.x - layout.size.width / 2f, pos.y - layout.size.height / 2f),
                )
            }

            drawArc(
                color = Ac42Colors.Red.copy(alpha = 0.62f),
                startAngle = -58f,
                sweepAngle = 50f,
                useCenter = false,
                topLeft = Offset(c.x - r, c.y - r),
                size = Size(r * 2, r * 2),
                style = Stroke(width = 3.5.dp.toPx(), cap = StrokeCap.Round),
            )
            drawArc(
                color = Ac42Colors.Green.copy(alpha = 0.62f),
                startAngle = 8f,
                sweepAngle = 50f,
                useCenter = false,
                topLeft = Offset(c.x - r, c.y - r),
                size = Size(r * 2, r * 2),
                style = Stroke(width = 3.5.dp.toPx(), cap = StrokeCap.Round),
            )

            if (connected && target != null && view.engaged && view.modeKey != "wind") {
                rotate(target.toFloat(), c) {
                    val y = c.y - r * 0.97f
                    val half = r * 0.026f
                    val path = Path().apply {
                        moveTo(c.x, y - half)
                        lineTo(c.x + half, y + half * 0.4f)
                        lineTo(c.x, y + half * 1.7f)
                        lineTo(c.x - half, y + half * 0.4f)
                        close()
                    }
                    drawPath(path, Ac42Colors.Amber)
                }
            }

            val heading = view.heading?.toFloat()
            if (connected && heading != null) {
                rotate(heading, c) {
                    drawLine(
                        color = Ac42Colors.Cyan,
                        start = c,
                        end = Offset(c.x, c.y - r * 0.81f),
                        strokeWidth = 3.dp.toPx(),
                        cap = StrokeCap.Round,
                    )
                    val tip = Offset(c.x, c.y - r * 0.88f)
                    val path = Path().apply {
                        moveTo(tip.x, tip.y)
                        lineTo(tip.x + 7.dp.toPx(), tip.y + 17.dp.toPx())
                        lineTo(tip.x - 7.dp.toPx(), tip.y + 17.dp.toPx())
                        close()
                    }
                    drawPath(path, Ac42Colors.Cyan)
                }
                val awa = view.awa
                if (awa != null) {
                    rotate(heading + awa.toFloat(), c) {
                        drawLine(
                            color = Ac42Colors.Violet,
                            start = Offset(c.x, c.y - r * 0.48f),
                            end = Offset(c.x, c.y - r * 0.66f),
                            strokeWidth = 2.5.dp.toPx(),
                            cap = StrokeCap.Round,
                        )
                        val tip = Offset(c.x, c.y - r * 0.72f)
                        val path = Path().apply {
                            moveTo(tip.x, tip.y)
                            lineTo(tip.x + 5.dp.toPx(), tip.y + 9.dp.toPx())
                            lineTo(tip.x - 5.dp.toPx(), tip.y + 9.dp.toPx())
                            close()
                        }
                        drawPath(path, Ac42Colors.Violet)
                    }
                }
            }

            drawCircle(Ac42Colors.Background.copy(alpha = 0.92f), radius = r * 0.55f, center = c)
            drawCircle(Ac42Colors.Line, radius = r * 0.55f, center = c, style = Stroke(1.3.dp.toPx()))
        }

        Column(
            modifier = Modifier.align(Alignment.Center).padding(horizontal = 8.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                view.modeLabel,
                color = Ac42Colors.Muted2,
                fontSize = 11.sp,
                fontWeight = FontWeight.Black,
                letterSpacing = 2.sp,
                maxLines = 1,
            )
            val value = bigValue(view, connected)
            Row(verticalAlignment = Alignment.Top) {
                Text(
                    fmt(value, 0),
                    color = Ac42Colors.Text,
                    fontSize = bigSp.sp,
                    fontWeight = FontWeight.Bold,
                    letterSpacing = (-1).sp,
                    maxLines = 1,
                )
                Text(
                    "°",
                    modifier = Modifier.padding(top = (bigSp * 0.16f).dp),
                    color = Ac42Colors.Muted2,
                    fontSize = (bigSp * 0.36f).sp,
                    fontWeight = FontWeight.Medium,
                )
            }
            Text(
                sideText(view, connected),
                color = Ac42Colors.Muted2,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 0.9.sp,
                maxLines = 1,
                textAlign = TextAlign.Center,
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Pilot card
// ---------------------------------------------------------------------------

@Composable
fun PilotCard(
    state: UiState,
    modifier: Modifier = Modifier,
    onMode: (String) -> Unit,
    onResponse: () -> Unit,
) {
    val view = buildPilotView(state)
    val connected = state.connected
    val controllerReady = state.status?.controller?.present == true
    val commandEnabled = connected && controllerReady && !state.commandBusy
    val responseEnabled = commandEnabled && view.engaged

    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(22.dp),
        color = Ac42Colors.Panel,
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(11.dp),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    "PILOTE AUTOMATIQUE",
                    color = Ac42Colors.Muted,
                    fontSize = 9.sp,
                    fontWeight = FontWeight.Black,
                    letterSpacing = 1.6.sp,
                )
                Text(
                    if (!connected) "HORS LIGNE" else view.modeLabel,
                    color = Ac42Colors.Cyan,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Black,
                    letterSpacing = 1.2.sp,
                )
            }

            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                ModeButton(
                    "STANDBY", "débrayé",
                    active = connected && (!view.engaged || view.modeKey == "standby" || view.modeKey == "nfu"),
                    enabled = commandEnabled,
                    activeColor = Color(0xFF47606B),
                    activeBackground = Ac42Colors.Panel3,
                    modifier = Modifier.weight(1f),
                ) { onMode("standby") }
                ModeButton(
                    "AUTO", "maintien de cap",
                    active = connected && view.modeKey == "auto",
                    enabled = commandEnabled,
                    activeColor = Ac42Colors.Cyan,
                    activeBackground = Ac42Colors.CyanDim,
                    modifier = Modifier.weight(1f),
                ) { onMode("auto") }
                ModeButton(
                    "VENT", "angle apparent",
                    active = connected && view.modeKey == "wind",
                    enabled = commandEnabled,
                    activeColor = Ac42Colors.Green,
                    activeBackground = Ac42Colors.GreenDim,
                    modifier = Modifier.weight(1f),
                ) { onMode("wind") }
            }

            ResponseCard(view.response, responseEnabled, onResponse)
            WindBar(view, connected)
            HeadingTrend(state.headingHistory)
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                InfoBox("CIBLE", targetLabel(view), Modifier.weight(1f))
                InfoBox("VENT", windLabel(view), Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun ModeButton(
    label: String,
    subtitle: String,
    active: Boolean,
    enabled: Boolean,
    activeColor: Color,
    activeBackground: Color,
    modifier: Modifier = Modifier,
    onClick: () -> Unit,
) {
    val border = if (active) activeColor else Ac42Colors.Line
    val background = if (active) activeBackground else Ac42Colors.Panel.copy(alpha = 0.85f)
    val textColor = if (active) Color.White else Ac42Colors.Muted2
    Box(
        modifier = modifier
            .height(76.dp)
            .alpha(if (enabled) 1f else 0.35f)
            .clip(RoundedCornerShape(15.dp))
            .background(background)
            .border(1.dp, border, RoundedCornerShape(15.dp))
            .clickable(enabled = enabled, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
            Text(label, color = textColor, fontSize = 14.sp, fontWeight = FontWeight.Black, letterSpacing = 0.5.sp)
            Text(subtitle, color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Bold, letterSpacing = 0.4.sp)
        }
    }
}

@Composable
private fun ResponseCard(response: ResponseState?, enabled: Boolean, onClick: () -> Unit) {
    val valid = response?.valid == true
    val setLabel = if (valid) {
        if (response?.set == "high") "HI" else "LO"
    } else "—"
    val modeLabel = if (valid) {
        if (response?.auto == true) "AUTO" else "MAN"
    } else ""
    val level = if (valid) response?.level?.let { "$it/10" } ?: "—" else "—"
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .alpha(if (enabled) 1f else 0.4f)
            .clip(RoundedCornerShape(13.dp))
            .clickable(enabled = enabled, onClick = onClick),
        shape = RoundedCornerShape(13.dp),
        color = Ac42Colors.Panel2.copy(alpha = 0.7f),
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 9.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("RÉPONSE", color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Black, letterSpacing = 1.2.sp)
            Spacer(Modifier.weight(1f))
            Text(setLabel, color = Ac42Colors.Amber, fontSize = 20.sp, fontWeight = FontWeight.Black)
            Spacer(Modifier.width(5.dp))
            Text(modeLabel, color = Ac42Colors.Muted2, fontSize = 8.sp, fontWeight = FontWeight.Bold, letterSpacing = 0.7.sp)
            Spacer(Modifier.width(9.dp))
            Text(level, color = Ac42Colors.Muted2, fontSize = 10.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
private fun WindBar(view: PilotView, connected: Boolean) {
    val angle = if (connected) view.awa else null
    val target = if (connected) view.windTarget else null
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(13.dp))
            .background(Ac42Colors.Panel2.copy(alpha = 0.55f))
            .border(1.dp, Ac42Colors.Line, RoundedCornerShape(13.dp))
            .padding(horizontal = 11.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(5.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text("ANGLE AU VENT", color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Black, letterSpacing = 1.2.sp)
            Text(
                angle?.let { "${fmt(abs(it), 0)}° ${if (it >= 0) "S" else "P"}" } ?: "—",
                color = Ac42Colors.Violet,
                fontSize = 14.sp,
                fontWeight = FontWeight.Black,
            )
        }
        Box(Modifier.fillMaxWidth().height(25.dp)) {
            Canvas(Modifier.fillMaxSize()) {
                val trackHeight = 20.dp.toPx()
                val top = (size.height - trackHeight) / 2f
                drawRoundRect(
                    color = Ac42Colors.Background.copy(alpha = 0.75f),
                    topLeft = Offset(0f, top),
                    size = Size(size.width, trackHeight),
                    cornerRadius = CornerRadius(7.dp.toPx()),
                )
                drawLine(Ac42Colors.Muted.copy(alpha = 0.5f), Offset(size.width / 2, top), Offset(size.width / 2, top + trackHeight), 1.dp.toPx())
                fun xFor(deg: Float): Float {
                    val f = 0.5f + (deg / 180f).coerceIn(-1f, 1f) * 0.47f
                    return size.width * f
                }
                target?.let {
                    drawLine(Ac42Colors.Amber.copy(alpha = 0.7f), Offset(xFor(it.toFloat()), top + 2.dp.toPx()), Offset(xFor(it.toFloat()), top + trackHeight - 2.dp.toPx()), 4.dp.toPx(), StrokeCap.Round)
                }
                angle?.let {
                    drawLine(Ac42Colors.Violet, Offset(xFor(it.toFloat()), top + 2.dp.toPx()), Offset(xFor(it.toFloat()), top + trackHeight - 2.dp.toPx()), 4.dp.toPx(), StrokeCap.Round)
                }
            }
            Text("P", modifier = Modifier.align(Alignment.CenterStart).padding(start = 5.dp), color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Black)
            Text("S", modifier = Modifier.align(Alignment.CenterEnd).padding(end = 5.dp), color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Black)
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(
                when {
                    angle == null -> "—"
                    angle >= 0 -> "AWA · TRIBORD"
                    else -> "AWA · BÂBORD"
                },
                color = Ac42Colors.Muted,
                fontSize = 7.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 0.6.sp,
            )
            Text(
                if (connected && view.modeKey == "wind") "CIBLE VENT" else "VENT APP.",
                color = Ac42Colors.Muted,
                fontSize = 7.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 0.6.sp,
            )
        }
    }
}

@Composable
private fun HeadingTrend(history: List<HeadingSample>) {
    val last = history.lastOrNull()
    val first = history.firstOrNull()
    val delta = if (last != null && first != null) wrap180(last.deg.toDouble() - first.deg) else null
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(13.dp))
            .background(Ac42Colors.Panel2.copy(alpha = 0.45f))
            .border(1.dp, Ac42Colors.Line, RoundedCornerShape(13.dp))
            .padding(horizontal = 11.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text("CAP · 60 S", color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Black, letterSpacing = 1.2.sp)
            Text(
                if (last == null) "—" else "${fmt(last.deg.toDouble(), 0)}° · ${if ((delta ?: 0.0) >= 0) "+" else ""}${fmt(delta, 1)}°",
                color = Ac42Colors.Cyan,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold,
            )
        }
        Canvas(Modifier.fillMaxWidth().height(58.dp)) {
            if (history.size < 2) return@Canvas
            val now = System.currentTimeMillis()
            val start = now - 60_000.0
            var previous = history.first().deg.toDouble()
            val points = history.map { sample ->
                var value = sample.deg.toDouble()
                while (value - previous > 180.0) value -= 360.0
                while (value - previous < -180.0) value += 360.0
                previous = value
                sample.timeMs.toDouble() to value
            }
            val values = points.map { it.second }
            val minValue = values.minOrNull() ?: 0.0
            val maxValue = values.maxOrNull() ?: 0.0
            val centerValue = (minValue + maxValue) / 2.0
            val half = max(10.0, (maxValue - minValue) / 2.0 + 2.0)
            fun x(t: Double): Float = (5f + ((t - start) / 60_000.0).toFloat() * (size.width - 10f))
            fun y(v: Double): Float = (7f + (0.5 - (v - centerValue) / (2 * half)).toFloat() * (size.height - 14f))
            for (k in -1..1) {
                val gy = y(centerValue + k * half * 0.7)
                drawLine(Ac42Colors.Line, Offset(5f, gy), Offset(size.width - 5f, gy), 1.dp.toPx(), pathEffect = null)
            }
            val path = Path()
            points.forEachIndexed { index, (t, v) ->
                val px = x(t)
                val py = y(v)
                if (index == 0) path.moveTo(px, py) else path.lineTo(px, py)
            }
            drawPath(path, Ac42Colors.Cyan, style = Stroke(width = 1.8.dp.toPx(), cap = StrokeCap.Round, join = androidx.compose.ui.graphics.StrokeJoin.Round))
            val lastPoint = points.last()
            drawCircle(Ac42Colors.Cyan, radius = 2.2.dp.toPx(), center = Offset(x(lastPoint.first), y(lastPoint.second)))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("−60 s", color = Ac42Colors.Muted, fontSize = 7.sp)
            Text("maintenant", color = Ac42Colors.Muted, fontSize = 7.sp)
        }
    }
}

@Composable
private fun InfoBox(label: String, value: String, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(Ac42Colors.Background.copy(alpha = 0.55f))
            .border(1.dp, Ac42Colors.Line, RoundedCornerShape(12.dp))
            .padding(horizontal = 9.dp, vertical = 7.dp),
    ) {
        Text(label, color = Ac42Colors.Muted, fontSize = 7.sp, fontWeight = FontWeight.Black, letterSpacing = 1.sp)
        Text(value, color = Ac42Colors.Muted2, fontSize = 11.sp, fontWeight = FontWeight.Bold, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------

@Composable
fun ControlsCard(
    state: UiState,
    modifier: Modifier = Modifier,
    onCourse: (String, Int) -> Unit,
    onTack: (String) -> Unit,
    onStartNfu: (String) -> Unit,
    onStopNfu: () -> Unit,
) {
    val view = buildPilotView(state)
    val connected = state.connected
    val controllerReady = state.status?.controller?.present == true
    val baseEnabled = connected && controllerReady && !state.commandBusy
    val nfuMode = connected && !view.engaged
    val engagedReady = baseEnabled && view.engaged && view.modeKey != "nav"
    val hint = when {
        !connected -> "Hors ligne"
        !controllerReady -> "En attente du pupitre B&G"
        !view.engaged -> "Standby · barre directe disponible"
        view.modeKey == "nav" -> "Nav · corrections désactivées"
        else -> "Pilotage actif · corrections disponibles"
    }

    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(20.dp),
        color = Ac42Colors.Panel,
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
    ) {
        Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(9.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("BARRE & CAP", color = Ac42Colors.Muted2, fontSize = 10.sp, fontWeight = FontWeight.Black, letterSpacing = 1.4.sp)
                Text(hint, color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Bold)
            }
            RudderBar(state.status?.instruments?.rudderDeg)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                CourseButton(
                    side = "port", degrees = 10,
                    nfuMode = nfuMode, enabled = if (nfuMode) baseEnabled else engagedReady,
                    nfuActive = state.nfuActive && state.nfuSide == "port",
                    modifier = Modifier.weight(1f),
                    onCourse = onCourse, onStartNfu = onStartNfu, onStopNfu = onStopNfu,
                )
                CourseButton(
                    side = "port", degrees = 1,
                    nfuMode = nfuMode, enabled = engagedReady,
                    nfuActive = false,
                    modifier = Modifier.weight(1f),
                    onCourse = onCourse, onStartNfu = onStartNfu, onStopNfu = onStopNfu,
                )
                CourseButton(
                    side = "stbd", degrees = 1,
                    nfuMode = nfuMode, enabled = engagedReady,
                    nfuActive = false,
                    modifier = Modifier.weight(1f),
                    onCourse = onCourse, onStartNfu = onStartNfu, onStopNfu = onStopNfu,
                )
                CourseButton(
                    side = "stbd", degrees = 10,
                    nfuMode = nfuMode, enabled = if (nfuMode) baseEnabled else engagedReady,
                    nfuActive = state.nfuActive && state.nfuSide == "stbd",
                    modifier = Modifier.weight(1f),
                    onCourse = onCourse, onStartNfu = onStartNfu, onStopNfu = onStopNfu,
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TackButton(
                    side = "port", label = "VIRER BÂBORD",
                    enabled = engagedReady, modifier = Modifier.weight(1f), onTack = onTack,
                )
                TackButton(
                    side = "stbd", label = "VIRER TRIBORD",
                    enabled = engagedReady, modifier = Modifier.weight(1f), onTack = onTack,
                )
            }
            Text(
                "Virement : appui long 2 s. En standby, les boutons ±10° deviennent une barre directe à maintenir.",
                color = Ac42Colors.Muted,
                fontSize = 8.sp,
                lineHeight = 11.sp,
            )
        }
    }
}

@Composable
private fun RudderBar(rudder: Double?) {
    val value = rudder?.coerceIn(-45.0, 45.0)
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("BÂBORD", color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Bold, letterSpacing = 0.7.sp)
            Text(
                if (value == null) "—" else "${abs(value).roundInt()}°${if (abs(value) < 0.5) "" else if (value > 0) " S" else " P"}",
                color = Ac42Colors.Text,
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
            )
            Text("TRIBORD", color = Ac42Colors.Muted, fontSize = 8.sp, fontWeight = FontWeight.Bold, letterSpacing = 0.7.sp)
        }
        Canvas(Modifier.fillMaxWidth().height(23.dp)) {
            val h = size.height
            drawRoundRect(
                color = Ac42Colors.Background.copy(alpha = 0.7f),
                cornerRadius = CornerRadius(8.dp.toPx()),
                size = Size(size.width, h),
            )
            drawLine(Ac42Colors.Muted.copy(alpha = 0.55f), Offset(size.width / 2, 2.dp.toPx()), Offset(size.width / 2, h - 2.dp.toPx()), 1.dp.toPx())
            if (value == null) return@Canvas
            val fraction = (abs(value) / 45.0).toFloat().coerceIn(0f, 1f)
            val width = (size.width / 2f) * fraction
            if (value >= 0) {
                drawRoundRect(
                    color = Ac42Colors.Green,
                    topLeft = Offset(size.width / 2, 3.dp.toPx()),
                    size = Size(width, h - 6.dp.toPx()),
                    cornerRadius = CornerRadius(5.dp.toPx()),
                )
            } else {
                drawRoundRect(
                    color = Ac42Colors.Red,
                    topLeft = Offset(size.width / 2 - width, 3.dp.toPx()),
                    size = Size(width, h - 6.dp.toPx()),
                    cornerRadius = CornerRadius(5.dp.toPx()),
                )
            }
        }
    }
}

private fun Double.roundInt(): Int = kotlin.math.round(this).toInt()

@Composable
private fun CourseButton(
    side: String,
    degrees: Int,
    nfuMode: Boolean,
    enabled: Boolean,
    nfuActive: Boolean,
    modifier: Modifier = Modifier,
    onCourse: (String, Int) -> Unit,
    onStartNfu: (String) -> Unit,
    onStopNfu: () -> Unit,
) {
    val isPort = side == "port"
    val nfu = nfuMode && degrees == 10
    val onClick = { onCourse(side, degrees) }
    var interaction: Modifier = Modifier
    if (nfu) {
        interaction = interaction.pointerInput(enabled) {
            if (!enabled) return@pointerInput
            detectTapGestures(
                onPress = {
                    onStartNfu(side)
                    tryAwaitRelease()
                    onStopNfu()
                },
            )
        }
    } else {
        interaction = interaction.clickable(enabled = enabled && !nfuMode, onClick = onClick)
    }
    val background = when {
        nfuActive -> Ac42Colors.AmberDim
        nfu -> Ac42Colors.Panel3
        isPort -> Ac42Colors.RedDim
        else -> Ac42Colors.GreenDim
    }
    val border = when {
        nfuActive -> Ac42Colors.Amber
        nfu -> Ac42Colors.Line2
        isPort -> Color(0xFF5E2029)
        else -> Color(0xFF15603A)
    }
    val textColor = when {
        nfuActive -> Ac42Colors.Amber
        nfu -> Ac42Colors.Amber
        isPort -> Color(0xFFFF9CA6)
        else -> Color(0xFF8FF3BA)
    }
    Box(
        modifier = modifier
            .height(66.dp)
            .alpha(if (enabled || nfu) 1f else 0.32f)
            .clip(RoundedCornerShape(14.dp))
            .background(background)
            .border(1.5.dp, border, RoundedCornerShape(14.dp))
            .then(interaction),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(
                when {
                    nfu -> "BARRE"
                    nfuMode && degrees == 1 -> "·"
                    isPort -> "−${degrees}°"
                    else -> "+${degrees}°"
                },
                color = textColor,
                fontSize = if (nfu) 15.sp else 20.sp,
                fontWeight = FontWeight.Black,
            )
            Text(
                when {
                    nfu -> if (isPort) "BÂBORD" else "TRIBORD"
                    nfuMode -> "NFU"
                    isPort -> "LOFER"
                    else -> "ABATTRE"
                },
                color = Ac42Colors.Muted,
                fontSize = 7.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 0.6.sp,
            )
        }
    }
}

@Composable
private fun TackButton(
    side: String,
    label: String,
    enabled: Boolean,
    modifier: Modifier = Modifier,
    onTack: (String) -> Unit,
) {
    var holding by remember { mutableStateOf(false) }
    var progress by remember { mutableFloatStateOf(0f) }
    val accent = if (side == "stbd") Ac42Colors.Green else Ac42Colors.Red
    val background = if (side == "stbd") Ac42Colors.GreenDim else Ac42Colors.RedDim

    LaunchedEffect(holding, enabled) {
        if (!holding || !enabled) {
            progress = 0f
            return@LaunchedEffect
        }
        val start = System.currentTimeMillis()
        while (holding && enabled) {
            val value = ((System.currentTimeMillis() - start) / 2000f).coerceIn(0f, 1f)
            progress = value
            if (value >= 1f) {
                holding = false
                progress = 0f
                onTack(side)
                break
            }
            delay(30)
        }
    }

    Box(
        modifier = modifier
            .height(45.dp)
            .alpha(if (enabled) 1f else 0.3f)
            .clip(RoundedCornerShape(12.dp))
            .background(Ac42Colors.Panel2.copy(alpha = 0.6f))
            .border(1.dp, if (holding) accent else Ac42Colors.Line, RoundedCornerShape(12.dp))
            .pointerInput(enabled) {
                if (!enabled) return@pointerInput
                detectTapGestures(
                    onPress = {
                        holding = true
                        tryAwaitRelease()
                        holding = false
                    },
                )
            },
        contentAlignment = Alignment.Center,
    ) {
        if (progress > 0f) {
            Box(
                Modifier
                    .fillMaxHeight()
                    .fillMaxWidth(progress)
                    .align(Alignment.CenterStart)
                    .background(background),
            )
        }
        Text(
            label,
            color = if (holding) accent else Ac42Colors.Muted2,
            fontSize = 8.sp,
            fontWeight = FontWeight.Black,
            letterSpacing = 0.8.sp,
        )
    }
}

// ---------------------------------------------------------------------------
// Instruments
// ---------------------------------------------------------------------------

@Composable
fun InstrumentsCard(state: UiState, modifier: Modifier = Modifier) {
    val status = state.status
    val inst = status?.instruments
    val view = buildPilotView(state)
    val heading = inst?.headingDeg
    val cog = inst?.cogDeg
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(20.dp),
        color = Ac42Colors.Panel,
        border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line),
    ) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("INSTRUMENTS", color = Ac42Colors.Muted2, fontSize = 10.sp, fontWeight = FontWeight.Black, letterSpacing = 1.4.sp)
                Text(
                    if (state.status?.simulated == true) "SIMULATION" else "NMEA 2000 · 250 kbit/s",
                    color = Ac42Colors.Muted,
                    fontSize = 8.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                InstrumentTile("CAP", fmt(heading, 0), "°${inst?.headingRef ?: ""}", Ac42Colors.Cyan, Modifier.weight(1f))
                InstrumentTile("VENT APP.", if (inst?.awaDeg == null) "—" else fmt(abs(inst.awaDeg), 0), "°", Ac42Colors.Amber, Modifier.weight(1f))
                InstrumentTile("VENT VRAI", if (inst?.twaDeg == null) "—" else fmt(abs(inst.twaDeg), 0), "°", Ac42Colors.Amber, Modifier.weight(1f))
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                InstrumentTile("TWS", fmt(inst?.twsKn, 1), "kn", Ac42Colors.Violet, Modifier.weight(1f))
                InstrumentTile("SOG", fmt(inst?.sogKn, 1), "kn", Ac42Colors.Green, Modifier.weight(1f))
                InstrumentTile("ROUTE", fmt(cog, 0), "°${inst?.cogRef ?: ""}", Ac42Colors.Cyan, Modifier.weight(1f))
            }
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 3.dp)
                    .border(1.dp, Ac42Colors.Line, RoundedCornerShape(1.dp))
                    .padding(top = 8.dp),
                verticalArrangement = Arrangement.spacedBy(3.dp),
            ) {
                DiagLine("AC42", status?.ap?.addressHex ?: if (status?.ap?.forced == true) "forcé" else "—")
                DiagLine("Pupitre", status?.controller?.addressHex ?: "—")
                DiagLine("Bus", status?.can?.state ?: "—")
                status?.let {
                    Text(
                        "Dernière trame : ${it.can.lastRxAgeMs?.let { age -> "$age ms" } ?: "—"}",
                        color = Ac42Colors.Muted,
                        fontSize = 8.sp,
                    )
                }
            }
        }
    }
}

@Composable
private fun InstrumentTile(label: String, value: String, unit: String, color: Color, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(11.dp))
            .background(Ac42Colors.Background.copy(alpha = 0.55f))
            .border(1.dp, Ac42Colors.Line, RoundedCornerShape(11.dp))
            .padding(vertical = 7.dp, horizontal = 5.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(label, color = Ac42Colors.Muted, fontSize = 7.sp, fontWeight = FontWeight.Black, letterSpacing = 0.6.sp, maxLines = 1)
        Text(value, color = color, fontSize = 20.sp, fontWeight = FontWeight.Bold, maxLines = 1)
        Text(unit, color = Ac42Colors.Muted2, fontSize = 7.sp, fontWeight = FontWeight.Bold)
    }
}

@Composable
private fun DiagLine(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, color = Ac42Colors.Muted, fontSize = 8.sp)
        Text(value, color = Ac42Colors.Muted2, fontSize = 8.sp, fontWeight = FontWeight.Bold)
    }
}

@Composable
fun Ac42Footer(state: UiState) {
    val version = state.status?.version ?: "—"
    val uptime = uptimeLabel(state.status?.uptimeS)
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text("Waveshare ESP32-S3-RS485-CAN · NMEA 2000", color = Ac42Colors.Muted, fontSize = 7.sp)
        Text("v$version · $uptime", color = Ac42Colors.Muted, fontSize = 7.sp)
    }
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

fun headingForGauge(state: UiState): Double? = buildPilotView(state).heading

fun targetForGauge(state: UiState): Double? = buildPilotView(state).target
