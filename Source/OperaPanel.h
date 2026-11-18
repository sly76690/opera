// ---- OPERA: the algorithm drawn live, and each operator's envelope ------------------------------------------------
// The algorithm: carriers (heard) on the bottom row in the section colour, each modulator on the row above what it
// modulates; lines show who modulates whom, a loop on operator 6 its feedback. Turning ALGORITHM redraws it.
class AlgorithmDiagram : public Component {
public:
 explicit AlgorithmDiagram(Orbit& o) : orbit(o) { setInterceptsMouseClicks(false, false); }
 void refresh() { const int a = int(orbit.state.getRawParameterValue("algorithm")->load()); const float fb = orbit.state.getRawParameterValue("feedback")->load();
  if (a != shown || std::abs(fb - shownFeedback) > .01f) { shown = a; shownFeedback = fb; repaint(); } }
 void paint(Graphics& g) override {
  using namespace orbitui; const auto& a = opera::algorithms()[size_t(jlimit(0, 15, shown))];
  // rows: a carrier is on row 0; a modulator one row above the highest of what it modulates
  std::array<int, opera::operators> row {}; int top = 0;
  for (int i = 0; i < opera::operators; ++i) { int r = 0; for (int t = 0; t < i; ++t) if (a.from[size_t(t)] & (1u << i)) r = jmax(r, row[size_t(t)] + 1); if (!a.carrier[size_t(i)] && r == 0) r = 1; row[size_t(i)] = r; top = jmax(top, r); }
  std::array<Rectangle<float>, opera::operators> box {};
  const float bw = 34, bh = 26, rowGap = jmin(44.f, (float(getHeight()) - 40.f) / float(jmax(1, top + 1)));
  for (int r = 0; r <= top; ++r) { std::vector<int> on; for (int i = 0; i < opera::operators; ++i) if (row[size_t(i)] == r) on.push_back(i);
   for (size_t k = 0; k < on.size(); ++k) { const float x = float(getWidth()) * (float(k) + .5f) / float(on.size()) - bw * .5f, y = float(getHeight()) - 34.f - float(r) * rowGap - bh;
    box[size_t(on[k])] = {x, y, bw, bh}; } }
  g.setColour(dim.withAlpha(.7f));
  for (int t = 0; t < opera::operators; ++t) for (int m = 0; m < opera::operators; ++m) if (a.from[size_t(t)] & (1u << m))
   g.drawLine(box[size_t(m)].getCentreX(), box[size_t(m)].getBottom(), box[size_t(t)].getCentreX(), box[size_t(t)].getY(), 1.6f);
  const float bus = float(getHeight()) - 16.f; float left = 1e9f, right = -1e9f;
  for (int i = 0; i < opera::operators; ++i) if (a.carrier[size_t(i)]) { g.drawLine(box[size_t(i)].getCentreX(), box[size_t(i)].getBottom(), box[size_t(i)].getCentreX(), bus, 1.6f); left = jmin(left, box[size_t(i)].getCentreX()); right = jmax(right, box[size_t(i)].getCentreX()); }
  g.drawLine(left, bus, right, bus, 1.6f); g.setFont(face(10.f, true)); g.drawText("OUT", Rectangle<float>(right + 4, bus - 7, 30, 14), Justification::centredLeft);
  { const auto b = box[5]; Path loop; loop.startNewSubPath(b.getRight(), b.getCentreY()); loop.cubicTo(b.getRight() + 16, b.getCentreY(), b.getRight() + 16, b.getY() - 12, b.getCentreX(), b.getY() - 12);
   loop.lineTo(b.getCentreX(), b.getY()); g.setColour(teal.withAlpha(.25f + .75f * shownFeedback)); g.strokePath(loop, PathStrokeType(1.6f)); }
  for (int i = 0; i < opera::operators; ++i) { const auto b = box[size_t(i)]; const bool heard = a.carrier[size_t(i)];
   g.setColour(heard ? teal : Colour(0xff2a2d30)); g.fillRoundedRectangle(b, 4); g.setColour(heard ? steelInk : ink); g.setFont(face(16.f, true)); g.drawText(String(i + 1), b, Justification::centred); }
 }
private:
 Orbit& orbit; int shown = -1; float shownFeedback = -1;
};

// One operator's envelope: the four levels, the times to reach them (drawn on a square-root scale so short times
// still show), the hold at level 3 while the key is down, then the release to level 4.
class EnvelopeGraph : public Component {
public:
 EnvelopeGraph(Orbit& o, int op) : orbit(o), index(op) { setInterceptsMouseClicks(false, false); }
 void refresh() { float sum = 0; for (int k = 0; k < 4; ++k) sum += value("t", k) * float(k + 1) + value("l", k) * float(k + 7); if (std::abs(sum - shownSum) > 1.e-5f) { shownSum = sum; repaint(); } }
 void paint(Graphics& g) override {
  using namespace orbitui; auto r = getLocalBounds().toFloat().reduced(2);
  g.setColour(Colour(0xff0e0f10)); g.fillRoundedRectangle(r, 3); r = r.reduced(4, 5);
  std::array<float, 4> t {}; float total = 0; for (int k = 0; k < 4; ++k) { t[size_t(k)] = std::sqrt(value("t", k)); total += t[size_t(k)]; }
  const float hold = total * .25f; total += hold;
  auto at = [&](float x, float level) { return Point<float>(r.getX() + r.getWidth() * x / total, r.getBottom() - r.getHeight() * level); };
  Path p; float x = 0; p.startNewSubPath(at(0, value("l", 3)));
  for (int k = 0; k < 3; ++k) { x += t[size_t(k)]; p.lineTo(at(x, value("l", k))); }
  x += hold; p.lineTo(at(x, value("l", 2))); x += t[3]; p.lineTo(at(x, value("l", 3)));
  g.setColour(teal); g.strokePath(p, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));
 }
private:
 float value(const char* what, int k) const { return orbit.state.getRawParameterValue(Orbit::opId(index, (what + String(k + 1)).toRawUTF8()))->load(); }
 Orbit& orbit; int index; float shownSum = -1;
};
