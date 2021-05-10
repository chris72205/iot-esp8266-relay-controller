#include "Arduino.h"
#include "Relay.h"

Relay::Relay(int controlPin) {
  _controlPin = controlPin;
  
  updatePin(_currentState);
}

// toggle current state and update the control pin
bool Relay::toggleState() {
  _currentState = !_currentState;
  updatePin(_currentState);
}

// get the current state of the control pin
bool Relay::getState() {
  return _currentState;
}

// set the state of the control pin
bool Relay::setState(bool state) {
  if(_currentState != state) {
    _currentState = state;
    updatePin(_currentState);
  }

  return _currentState;
}

// update the control pin output (if we're allowed to)
void Relay::updatePin(bool state) {
  unsigned long currentMillis = millis();

  // check if we can update the relay pin
  if(canUpdatePin(currentMillis)) {
    _currentState = state;
    _lastRelayChange = currentMillis;
    digitalWrite(_controlPin, _currentState);
  }

  // else do nothing, since we are within the debounce period
}

// check if we can update the control pin based on debounce rules
bool Relay::canUpdatePin(long currentMillis) {
  return (
    // we have rolled over and current time is outside of debouncePeriod, so we can do this
    (currentMillis < _lastRelayChange && currentMillis > _debouncePeriod)
    ||
    // current time is more than the debounce period since the last relay change, so we can do this
    (currentMillis > (_lastRelayChange + _debouncePeriod))
  );
}
