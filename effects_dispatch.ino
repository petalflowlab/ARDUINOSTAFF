void renderCurrentEffect() {
  uint32_t now = millis();
  float dt = clampf((now - lastUpdate) / 1000.0f, 0.0f, 0.1f);
  for (int i = HEAD_LENGTH; i < HEAD_LENGTH + STAFF_LENGTH; i++) leds[i] = CRGB::Black;
  if (currentPage == PAGE_1) {
    switch (currentEffect) {
      case 0: renderPurpleBlobEffect(dt);    break;
      case 1: renderRainbowPainterEffect(dt); break;
      case 2: renderFireStormEffect(dt);     break;
      case 3: renderOceanWavesEffect(dt);    break;
      case 4: renderCrystalPulseEffect(dt);  break;
      case 5: renderPingPongEffect(dt);      break;
    }
  } else if (currentPage == PAGE_2) {
    switch (currentEffect) {
      case 0: renderAudioPulseEffect(dt);   break;
      case 1: renderBeatSparkleEffect(dt);  break;
      case 2: renderSoundWaveEffect(dt);    break;
      case 3: renderColorShiftEffect(dt);   break;
      case 4: renderAudioFireEffect(dt);    break;
    }
  } else if (currentPage == PAGE_3) {
    switch (currentEffect) {
      case 0: renderPlasmaStormEffect(dt);     break;
      case 1: renderLightningStrikeEffect(dt);  break;
      case 2: renderCometTrailEffect(dt);      break;
      case 3: renderAuroraFlowEffect(dt);      break;
      case 4: renderGalaxySwirlEffect(dt);     break;
    }
  } else if (currentPage == PAGE_4) {
    switch (currentEffect) {
      case 0: renderOceanBreezeEffect(dt);       break;
      case 1: renderSunsetFadeEffect(dt);        break;
      case 2: renderForestMistEffect(dt);        break;
      case 3: renderAuroraDreamsEffect(dt);      break;
      case 4: renderLavaFlowEffect(dt);          break;
      case 5: renderSmokeyCloudstormEffect(dt);  break;
      case 6: renderDandelionSeedsEffect(dt);    break;
      case 7: renderTulipBouquetEffect(dt);      break;
      case 8: renderSpaceRocketEffect(dt);       break;
      case 9: renderPumpkinPatchEffect(dt);      break;
    }
  } else if (currentPage == PAGE_5) {
    switch (currentEffect) {
      case 0: renderWatermelonEffect(dt);   break;
      case 1: renderCitrusBurstEffect(dt);  break;
      case 2: renderBerryBlastEffect(dt);   break;
      case 3: renderMangoSwirlEffect(dt);   break;
      case 4: renderKiwiSparkEffect(dt);    break;
    }
  } else if (currentPage == PAGE_6) {
    switch (currentEffect) {
      case 0: renderNewtonsCradleEffect(dt);    break;
      case 1: renderSlinkyEffect(dt);           break;
      case 2: renderInkDropEffect(dt);          break;
      case 3: renderRippleTankEffect(dt);       break;
      case 4: renderSupernovaEffect(dt);        break;
      case 5: renderBioluminescenceEffect(dt);  break;
      case 6: renderPlasmaWavesEffect(dt);          break;
      case 7: renderElectronOrbitEffect(dt);        break;
      case 8: renderDNAHelixEffect(dt);             break;
      case 9: renderMeteorShowerEffect(dt);         break;
      case 10: renderMagneticPullEffect(dt);        break;
      case 11: renderSolarFlareEffect(dt);          break;
      case 12: renderMolecularVibrationEffect(dt);  break;
      case 13: renderBlackHoleEffect(dt);           break;
    }
  }
  lastUpdate = now;
}
