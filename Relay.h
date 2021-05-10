class Relay {
  public:
    Relay(int controlPin);
    bool toggleState();
    bool getState();
    bool setState(bool state);

  private:
    int _controlPin;
    bool _currentState = false;
    unsigned long _lastRelayChange;
    const unsigned int _debouncePeriod = 1000;

    void updatePin(bool state);
    bool canUpdatePin(long currentMillis);
};
