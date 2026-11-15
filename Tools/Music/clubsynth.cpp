// clubsynth: renders the club music stems for the adaptive music system (MUSIC.md).
//
// Original music in a dark synth / dark techno style: F minor, 125 BPM, 4/4. Every stem is a seamless loop of whole bars:
// each one is rendered over two passes of its pattern and the second pass is kept, so note tails, delays and reverbs from the
// end of the loop are already present at its start. At 48 kHz and 125 BPM a beat is exactly 23040 samples, so loops of
// 8, 16 and 32 bars stay sample-locked forever when the game starts them together.
//
// Plain C++17, no dependencies. Deterministic: the same build writes the same files.
//   Tools/Music/build.sh <exe>   then   <exe> <out dir>
// Writes one WAV per stem (16-bit PCM) and three preview mixes (prowl / hunt / fight arrangement, and a demo) to listen to.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	constexpr int SampleRate = 48000;
	constexpr int Bpm = 125;
	constexpr int SamplesPerBeat = SampleRate * 60 / Bpm; // 23040, exact
	constexpr int SamplesPerStep = SamplesPerBeat / 4;    // a 16th note
	constexpr int StepsPerBar = 16;
	constexpr int SamplesPerBar = SamplesPerBeat * 4;
	constexpr double Pi = 3.14159265358979323846;
	constexpr double TwoPi = 2.0 * Pi;

	static_assert(SampleRate * 60 % Bpm == 0, "a beat must be a whole number of samples");

	// ---- small utilities --------------------------------------------------------------------------------------------------

	struct FRng
	{
		uint64_t State;
		explicit FRng(uint64_t Seed) : State(Seed) {}
		uint64_t Next()
		{
			uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}
		double Unit() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
		double Signed() { return Unit() * 2.0 - 1.0; }
	};

	double MidiHz(double Note) { return 440.0 * std::pow(2.0, (Note - 69.0) / 12.0); }
	double Clamp(double X, double Lo, double Hi) { return std::min(Hi, std::max(Lo, X)); }
	double Db(double Decibels) { return std::pow(10.0, Decibels / 20.0); }

	struct FBuffer
	{
		std::vector<double> L, R;
		FBuffer() = default;
		explicit FBuffer(size_t N) : L(N, 0.0), R(N, 0.0) {}
		size_t Size() const { return L.size(); }
		void Add(size_t I, double Left, double Right)
		{
			if (I < L.size()) { L[I] += Left; R[I] += Right; }
		}
		void Mix(const FBuffer& Other, double Gain)
		{
			for (size_t I = 0; I < std::min(Size(), Other.Size()); ++I) { L[I] += Other.L[I] * Gain; R[I] += Other.R[I] * Gain; }
		}
	};

	// PolyBLEP band-limited saw and square.
	double PolyBlep(double T, double Dt)
	{
		if (T < Dt) { T /= Dt; return T + T - T * T - 1.0; }
		if (T > 1.0 - Dt) { T = (T - 1.0) / Dt; return T * T + T + T + 1.0; }
		return 0.0;
	}
	double Saw(double Phase, double Dt) { return 2.0 * Phase - 1.0 - PolyBlep(Phase, Dt); }
	double Square(double Phase, double Dt)
	{
		double V = Phase < 0.5 ? 1.0 : -1.0;
		V += PolyBlep(Phase, Dt);
		V -= PolyBlep(std::fmod(Phase + 0.5, 1.0), Dt);
		return V;
	}

	// Zero-delay-feedback state variable filter (Simper). Cutoff in Hz, Q around 0.5 (soft) to 10 (screaming).
	struct FSvf
	{
		double Ic1 = 0.0, Ic2 = 0.0;
		double Lp = 0.0, Bp = 0.0, Hp = 0.0;
		void Run(double X, double CutoffHz, double Q)
		{
			const double G = std::tan(Pi * Clamp(CutoffHz, 10.0, SampleRate * 0.45) / SampleRate);
			const double K = 1.0 / Q;
			const double A1 = 1.0 / (1.0 + G * (G + K));
			const double A2 = G * A1;
			const double A3 = G * A2;
			const double V3 = X - Ic2;
			const double V1 = A1 * Ic1 + A2 * V3;
			const double V2 = Ic2 + A2 * Ic1 + A3 * V3;
			Ic1 = 2.0 * V1 - Ic1;
			Ic2 = 2.0 * V2 - Ic2;
			Lp = V2;
			Bp = V1;
			Hp = X - K * V1 - V2;
		}
	};

	double Drive(double X, double Amount) { return std::tanh(X * Amount) / std::tanh(Amount); }

	// Freeverb-style reverb (8 combs + 4 allpasses per channel).
	struct FReverb
	{
		struct FComb
		{
			std::vector<double> Buf;
			size_t Pos = 0;
			double Store = 0.0;
			double Run(double X, double Feedback, double Damp)
			{
				const double Out = Buf[Pos];
				Store = Out * (1.0 - Damp) + Store * Damp;
				Buf[Pos] = X + Store * Feedback;
				Pos = (Pos + 1) % Buf.size();
				return Out;
			}
		};
		struct FAllpass
		{
			std::vector<double> Buf;
			size_t Pos = 0;
			double Run(double X)
			{
				const double B = Buf[Pos];
				Buf[Pos] = X + B * 0.5;
				Pos = (Pos + 1) % Buf.size();
				return B - X;
			}
		};
		FComb CombL[8], CombR[8];
		FAllpass ApL[4], ApR[4];
		double Feedback, Damp;

		FReverb(double RoomSize, double InDamp) : Feedback(0.7 + 0.28 * RoomSize), Damp(InDamp)
		{
			const int Combs[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
			const int Aps[4] = {556, 441, 341, 225};
			const double Scale = SampleRate / 44100.0;
			for (int I = 0; I < 8; ++I)
			{
				CombL[I].Buf.assign(static_cast<size_t>(Combs[I] * Scale), 0.0);
				CombR[I].Buf.assign(static_cast<size_t>((Combs[I] + 23) * Scale), 0.0);
			}
			for (int I = 0; I < 4; ++I)
			{
				ApL[I].Buf.assign(static_cast<size_t>(Aps[I] * Scale), 0.0);
				ApR[I].Buf.assign(static_cast<size_t>((Aps[I] + 23) * Scale), 0.0);
			}
		}
		void Run(double InL, double InR, double& OutL, double& OutR)
		{
			const double In = (InL + InR) * 0.015;
			OutL = OutR = 0.0;
			for (int I = 0; I < 8; ++I) { OutL += CombL[I].Run(In, Feedback, Damp); OutR += CombR[I].Run(In, Feedback, Damp); }
			for (int I = 0; I < 4; ++I) { OutL = ApL[I].Run(OutL); OutR = ApR[I].Run(OutR); }
		}
	};

	// Adds a reverb of the buffer to itself (Wet = send level).
	void ApplyReverb(FBuffer& B, double RoomSize, double Damp, double Wet, double LowCutHz = 200.0)
	{
		FReverb Verb(RoomSize, Damp);
		FSvf HpL, HpR;
		for (size_t I = 0; I < B.Size(); ++I)
		{
			double L, R;
			Verb.Run(B.L[I], B.R[I], L, R);
			HpL.Run(L, LowCutHz, 0.7);
			HpR.Run(R, LowCutHz, 0.7);
			B.L[I] += HpL.Hp * Wet;
			B.R[I] += HpR.Hp * Wet;
		}
	}

	// Ping-pong delay in 16th steps, filtered feedback.
	void ApplyDelay(FBuffer& B, int Steps, double Feedback, double Wet, double ToneHz = 3500.0)
	{
		const size_t D = static_cast<size_t>(Steps) * SamplesPerStep;
		std::vector<double> BufL(D, 0.0), BufR(D, 0.0);
		FSvf ToneL, ToneR;
		size_t Pos = 0;
		for (size_t I = 0; I < B.Size(); ++I)
		{
			const double OutL = BufL[Pos], OutR = BufR[Pos];
			ToneL.Run(OutL, ToneHz, 0.7);
			ToneR.Run(OutR, ToneHz, 0.7);
			// Ping-pong: the left input feeds the right line and back.
			BufL[Pos] = (B.L[I] + B.R[I]) * 0.5 + ToneR.Lp * Feedback;
			BufR[Pos] = ToneL.Lp * Feedback;
			Pos = (Pos + 1) % D;
			B.L[I] += OutL * Wet;
			B.R[I] += OutR * Wet;
		}
	}

	// Baked-in sidechain: the stem ducks on every beat as if the kick were compressing it, so the groove pumps even when the
	// kick stem is silent.
	void ApplyPump(FBuffer& B, double Depth, double ReleaseSeconds)
	{
		for (size_t I = 0; I < B.Size(); ++I)
		{
			const double T = static_cast<double>(I % SamplesPerBeat) / SampleRate;
			const double Attack = Clamp(T / 0.004, 0.0, 1.0); // 4 ms into the duck, no click
			const double Gain = 1.0 - Depth * Attack * std::exp(-T / ReleaseSeconds);
			B.L[I] *= Gain;
			B.R[I] *= Gain;
		}
	}

	void HighPass(FBuffer& B, double Hz)
	{
		FSvf L, R;
		for (size_t I = 0; I < B.Size(); ++I) { L.Run(B.L[I], Hz, 0.7); R.Run(B.R[I], Hz, 0.7); B.L[I] = L.Hp; B.R[I] = R.Hp; }
	}

	// ---- music ------------------------------------------------------------------------------------------------------------

	// The 16-bar progression, one chord per 2 bars: Fm | Db | Eb | C | Fm | Db | Bbm | C. Every harmonic stem follows it, so
	// any combination of stems and variants fits.
	struct FChord
	{
		int Root;  // bass register (MIDI), F2 = 41
		bool bMinor;
		int Pad[4];
	};
	const FChord Progression[8] = {
		{41, true, {53, 56, 60, 65}},  // Fm
		{37, false, {49, 56, 61, 65}}, // Db
		{39, false, {51, 55, 58, 63}}, // Eb
		{36, false, {48, 55, 60, 64}}, // C
		{41, true, {53, 56, 60, 65}},  // Fm
		{37, false, {49, 56, 61, 65}}, // Db
		{34, true, {46, 53, 58, 61}},  // Bbm
		{36, false, {48, 55, 60, 64}}, // C
	};
	const FChord& ChordAtBar(int Bar) { return Progression[(Bar / 2) % 8]; }
	int Third(const FChord& C) { return C.bMinor ? 3 : 4; }

	size_t StepSample(int Step) { return static_cast<size_t>(Step) * SamplesPerStep; }

	struct FNote
	{
		int Step;   // start, in 16ths from the loop start
		int Length; // in 16ths
		double Note;
		double Velocity;
	};

	// Renders a stem's pattern twice and keeps the second pass, so the loop is seamless.
	struct FStem
	{
		std::string Name;
		int Bars;
		bool bMono;
		double TargetRmsDb; // mix balance: loudness of the stem alone while it plays
		FBuffer Audio;
	};

	// ---- drums ----

	void Kick(FBuffer& Out, size_t At, double Vel)
	{
		const size_t N = static_cast<size_t>(0.5 * SampleRate);
		double Phase = 0.0;
		FRng Noise(At + 1);
		for (size_t I = 0; I < N; ++I)
		{
			const double T = static_cast<double>(I) / SampleRate;
			const double Freq = 47.0 + 150.0 * std::exp(-T / 0.032) + 600.0 * std::exp(-T / 0.003);
			Phase += Freq / SampleRate;
			const double Body = std::sin(TwoPi * Phase) * std::exp(-T / 0.32) * Clamp(T / 0.001, 0.0, 1.0);
			const double Click = Noise.Signed() * std::exp(-T / 0.0015) * 0.35;
			const double V = Drive((Body + Click) * Vel, 2.2) * 0.95;
			Out.Add(At + I, V, V);
		}
	}

	void Hat(FBuffer& Out, size_t At, double Vel, double DecaySeconds, double Pan, FRng& Rng)
	{
		const size_t N = static_cast<size_t>(DecaySeconds * 6.0 * SampleRate);
		FSvf Hp, Bp;
		for (size_t I = 0; I < N; ++I)
		{
			const double T = static_cast<double>(I) / SampleRate;
			const double X = Rng.Signed();
			Hp.Run(X, 7500.0, 0.8);
			Bp.Run(X, 10500.0, 1.5);
			const double V = (Hp.Hp * 0.7 + Bp.Bp * 0.5) * std::exp(-T / DecaySeconds) * Vel;
			Out.Add(At + I, V * (1.0 - Pan), V * (1.0 + Pan));
		}
	}

	void Clap(FBuffer& Out, size_t At, double Vel, FRng& Rng)
	{
		const size_t N = static_cast<size_t>(0.35 * SampleRate);
		FSvf Bp;
		for (size_t I = 0; I < N; ++I)
		{
			const double T = static_cast<double>(I) / SampleRate;
			// Three quick bursts, then the body.
			double Env = std::exp(-T / 0.09);
			for (int K = 0; K < 3; ++K)
			{
				const double Tk = T - K * 0.011;
				if (Tk >= 0.0 && Tk < 0.011) { Env = std::max(Env, std::exp(-Tk / 0.004)); }
			}
			Bp.Run(Rng.Signed(), 1350.0, 1.2);
			const double V = Bp.Bp * Env * Vel * 2.2;
			Out.Add(At + I, V, V);
		}
	}

	void Snare(FBuffer& Out, size_t At, double Vel, FRng& Rng)
	{
		const size_t N = static_cast<size_t>(0.2 * SampleRate);
		FSvf Hp;
		double Phase = 0.0;
		for (size_t I = 0; I < N; ++I)
		{
			const double T = static_cast<double>(I) / SampleRate;
			Phase += (190.0 + 60.0 * std::exp(-T / 0.01)) / SampleRate;
			Hp.Run(Rng.Signed(), 1800.0, 0.7);
			const double V = (std::sin(TwoPi * Phase) * std::exp(-T / 0.03) * 0.6 + Hp.Hp * std::exp(-T / 0.06)) * Vel;
			Out.Add(At + I, V, V);
		}
	}

	// A short metallic tick (pipe hit): two inharmonic sines.
	void Rim(FBuffer& Out, size_t At, double Vel, double Pan)
	{
		const size_t N = static_cast<size_t>(0.12 * SampleRate);
		for (size_t I = 0; I < N; ++I)
		{
			const double T = static_cast<double>(I) / SampleRate;
			const double V = (std::sin(TwoPi * 1730.0 * T) + 0.6 * std::sin(TwoPi * 2512.0 * T)) * std::exp(-T / 0.018) * Vel * 0.5;
			Out.Add(At + I, V * (1.0 - Pan), V * (1.0 + Pan));
		}
	}

	FBuffer RenderKick(int Bars)
	{
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Bar = 0; Bar < Bars; ++Bar)
			{
				const int Base = (Pass * Bars + Bar) * StepsPerBar;
				for (int Beat = 0; Beat < 4; ++Beat) { Kick(B, StepSample(Base + Beat * 4), 1.0); }
				if (Bar == Bars - 1) { Kick(B, StepSample(Base + 14), 0.7); } // turnaround into the next phrase
			}
		}
		HighPass(B, 22.0); // removes the DC the asymmetric drive leaves
		return B;
	}

	FBuffer RenderHats(int Bars)
	{
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			FRng Rng(7); // same humanization each pass: the second pass is the loop
			for (int Bar = 0; Bar < Bars; ++Bar)
			{
				const int Base = (Pass * Bars + Bar) * StepsPerBar;
				for (int S = 0; S < StepsPerBar; ++S)
				{
					if (S % 4 == 2)
					{
						Hat(B, StepSample(Base + S), 0.55, 0.075, 0.1, Rng); // open hat on the off-beat
						continue;
					}
					const double Vel = (S % 4 == 0 ? 0.32 : 0.22) + Rng.Signed() * 0.05;
					Hat(B, StepSample(Base + S), Vel, 0.018, -0.15 + 0.3 * (S % 2), Rng);
				}
				if (Bar == Bars - 1)
				{
					// 32nd-note roll on the last beat.
					for (int K = 0; K < 8; ++K)
					{
						Hat(B, StepSample(Base + 12) + static_cast<size_t>(K) * SamplesPerStep / 2, 0.15 + 0.03 * K, 0.015, 0.0, Rng);
					}
				}
			}
		}
		return B;
	}

	FBuffer RenderPerc(int Bars)
	{
		FBuffer Claps(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		FBuffer Rest(Claps.Size());
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			FRng Rng(11);
			for (int Bar = 0; Bar < Bars; ++Bar)
			{
				const int Base = (Pass * Bars + Bar) * StepsPerBar;
				Clap(Claps, StepSample(Base + 4), 0.9, Rng);
				Clap(Claps, StepSample(Base + 12), 0.9, Rng);
				Rim(Rest, StepSample(Base + 3), 0.5, -0.4);
				Rim(Rest, StepSample(Base + 11), 0.4, 0.4);
				if (Bar % 2 == 1) { Rim(Rest, StepSample(Base + 7), 0.35, 0.2); }
				if (Bar % 4 == 3) { Clap(Claps, StepSample(Base + 15), 0.35, Rng); }
				if (Bar == Bars - 1)
				{
					// Snare build into the next phrase: 16ths, then 32nds on the last beat.
					for (int S = 8; S < 12; ++S) { Snare(Rest, StepSample(Base + S), 0.25 + 0.06 * (S - 8), Rng); }
					for (int K = 0; K < 8; ++K)
					{
						Snare(Rest, StepSample(Base + 12) + static_cast<size_t>(K) * SamplesPerStep / 2, 0.45 + 0.05 * K, Rng);
					}
				}
			}
		}
		ApplyReverb(Claps, 0.55, 0.4, 0.9, 400.0);
		Claps.Mix(Rest, 1.0);
		return Claps;
	}

	// ---- bass ----

	// One bass note: two detuned saws through a resonant low-pass with its own envelope, then drive.
	void BassNote(FBuffer& Out, size_t At, size_t Length, double Note, double Vel, double CutBase, double CutEnv, double Res,
		double DecaySeconds, double DriveAmount)
	{
		const double F = MidiHz(Note);
		double P1 = 0.0, P2 = 0.37, P3 = 0.0;
		FSvf Filter;
		const size_t Release = static_cast<size_t>(0.012 * SampleRate);
		for (size_t I = 0; I < Length + Release; ++I)
		{
			const double T = static_cast<double>(I) / SampleRate;
			const double Gate = I < Length ? 1.0 : 1.0 - static_cast<double>(I - Length) / static_cast<double>(Release);
			const double Amp = Gate * Clamp(T / 0.002, 0.0, 1.0);
			const double D1 = F * 1.004 / SampleRate, D2 = F * 0.996 / SampleRate, D3 = F * 0.5 / SampleRate;
			P1 = std::fmod(P1 + D1, 1.0);
			P2 = std::fmod(P2 + D2, 1.0);
			P3 = std::fmod(P3 + D3, 1.0);
			const double X = Saw(P1, D1) + Saw(P2, D2) + 0.6 * std::sin(TwoPi * P3);
			Filter.Run(X, CutBase + CutEnv * std::exp(-T / DecaySeconds), Res);
			const double V = Drive(Filter.Lp * Vel * 0.6, DriveAmount) * Amp;
			Out.Add(At + I, V, V);
		}
	}

	// Bass A: the rolling gallop. Three 16ths in each beat, off the kick, with an octave kick-up at the end of each bar.
	FBuffer RenderBassA(int Bars)
	{
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Bar = 0; Bar < Bars; ++Bar)
			{
				const FChord& C = ChordAtBar(Bar);
				const int Base = (Pass * Bars + Bar) * StepsPerBar;
				for (int S = 0; S < StepsPerBar; ++S)
				{
					if (S % 4 == 0) { continue; }
					double Note = C.Root;
					if (S == 15) { Note += 12; }
					else if (S == 7 && Bar % 2 == 1) { Note += 7; }
					const double Accent = (S % 4 == 1) ? 1.0 : 0.8;
					BassNote(B, StepSample(Base + S), static_cast<size_t>(SamplesPerStep * 0.8), Note, Accent, 180.0, 1700.0 * Accent,
						1.6, 0.06, 2.5);
				}
			}
		}
		HighPass(B, 22.0); // removes the DC the asymmetric drive leaves
		return B;
	}

	// Bass B: off-beat 8ths with a resonant filter that opens over every two bars, plus octave pickups. Meaner, more acid.
	FBuffer RenderBassB(int Bars)
	{
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Bar = 0; Bar < Bars; ++Bar)
			{
				const FChord& C = ChordAtBar(Bar);
				const int Base = (Pass * Bars + Bar) * StepsPerBar;
				const double Sweep = 0.5 - 0.5 * std::cos(Pi * ((Bar % 2) + 0.5) / 2.0); // opens across the 2-bar chord
				for (int S = 2; S < StepsPerBar; S += 4)
				{
					BassNote(B, StepSample(Base + S), static_cast<size_t>(SamplesPerStep * 1.6), C.Root, 1.0, 220.0 + 500.0 * Sweep,
						1500.0 + 1500.0 * Sweep, 4.5, 0.09, 3.5);
				}
				BassNote(B, StepSample(Base + 7), static_cast<size_t>(SamplesPerStep * 0.7), C.Root + 12, 0.7, 300.0, 2200.0, 3.0, 0.05, 3.0);
				BassNote(B, StepSample(Base + 15), static_cast<size_t>(SamplesPerStep * 0.7), C.Root + (Bar % 2 ? 7 : 12), 0.7, 300.0,
					2200.0, 3.0, 0.05, 3.0);
			}
		}
		HighPass(B, 22.0); // removes the DC the asymmetric drive leaves
		return B;
	}

	// Sub: a clean sine an octave under the bass, one note per chord. Carries the low end when the bass is out.
	FBuffer RenderSub(int Bars)
	{
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Bar = 0; Bar < Bars; Bar += 2)
			{
				const FChord& C = ChordAtBar(Bar);
				const size_t At = StepSample((Pass * Bars + Bar) * StepsPerBar);
				const size_t Length = static_cast<size_t>(2 * SamplesPerBar);
				const double F = MidiHz(C.Root - 12);
				for (size_t I = 0; I < Length; ++I)
				{
					const double T = static_cast<double>(I) / SampleRate;
					const double Env = Clamp(T / 0.03, 0.0, 1.0) * Clamp((static_cast<double>(Length - I)) / (0.03 * SampleRate), 0.0, 1.0);
					const double V = Drive(std::sin(TwoPi * F * T) * 1.2, 1.3) * Env;
					B.Add(At + I, V, V);
				}
			}
		}
		ApplyPump(B, 0.7, 0.12);
		return B;
	}

	// ---- pads, drone, arp, leads ----

	// Dark detuned-saw chord pad, slow filter swell, wide and wet. 32 bars: the second 16 voiced an octave up and brighter.
	FBuffer RenderPad(int Bars)
	{
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		const double Detune[5] = {-0.11, -0.05, 0.0, 0.06, 0.12};
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Bar = 0; Bar < Bars; Bar += 2)
			{
				const FChord& C = ChordAtBar(Bar);
				const bool bHigh = Bar >= 16;
				const size_t At = StepSample((Pass * Bars + Bar) * StepsPerBar);
				const size_t Length = static_cast<size_t>(2 * SamplesPerBar);
				const size_t Release = static_cast<size_t>(0.9 * SampleRate);
				for (int V = 0; V < 4; ++V)
				{
					const double Note = C.Pad[V] + (bHigh && V > 0 ? 12 : 0);
					FSvf FilterL, FilterR;
					double Phase[5] = {0.1, 0.3, 0.5, 0.7, 0.9};
					for (size_t I = 0; I < Length + Release; ++I)
					{
						const double T = static_cast<double>(I) / SampleRate;
						const double Gate = I < Length ? 1.0 : std::exp(-static_cast<double>(I - Length) / (0.25 * SampleRate));
						const double Amp = Clamp(T / 0.6, 0.0, 1.0) * Gate;
						double L = 0.0, R = 0.0;
						for (int K = 0; K < 5; ++K)
						{
							const double Dt = MidiHz(Note + Detune[K]) / SampleRate;
							Phase[K] = std::fmod(Phase[K] + Dt, 1.0);
							const double S = Saw(Phase[K], Dt);
							L += S * (1.0 - 0.18 * (K - 2));
							R += S * (1.0 + 0.18 * (K - 2));
						}
						const double Swell = 0.5 - 0.5 * std::cos(Pi * Clamp(T / 3.84, 0.0, 1.0)); // opens over the 2 bars
						const double Cut = (bHigh ? 900.0 : 500.0) + (bHigh ? 1400.0 : 900.0) * Swell;
						FilterL.Run(L, Cut, 0.9);
						FilterR.Run(R, Cut, 0.9);
						B.Add(At + I, FilterL.Lp * Amp * 0.12, FilterR.Lp * Amp * 0.12);
					}
				}
			}
		}
		HighPass(B, 120.0);
		ApplyReverb(B, 0.85, 0.5, 1.4, 250.0);
		ApplyPump(B, 0.45, 0.16);
		return B;
	}

	// Tunnel drone: low detuned saws on F barely opened, plus filtered noise like air moving in a culvert.
	FBuffer RenderDrone(int Bars)
	{
		const size_t Loop = static_cast<size_t>(Bars) * SamplesPerBar;
		FBuffer B(2 * Loop + SamplesPerBeat);
		FSvf ToneL, ToneR, AirL, AirR;
		FRng Rng(23);
		double P[4] = {0.0, 0.25, 0.5, 0.75};
		const double Notes[4] = {29.0, 29.08, 41.0, 48.0}; // F1, F1 detuned, F2, C3
		double BrownL = 0.0, BrownR = 0.0;
		for (size_t I = 0; I < B.Size(); ++I)
		{
			// LFOs complete whole cycles per loop so the loop point is seamless.
			const double LoopPhase = static_cast<double>(I % Loop) / static_cast<double>(Loop);
			const double SlowA = 0.5 - 0.5 * std::cos(TwoPi * LoopPhase * 2.0);
			const double SlowB = 0.5 - 0.5 * std::cos(TwoPi * LoopPhase * 3.0 + 1.0);
			double L = 0.0, R = 0.0;
			for (int K = 0; K < 4; ++K)
			{
				const double Dt = MidiHz(Notes[K]) / SampleRate;
				P[K] = std::fmod(P[K] + Dt, 1.0);
				const double S = Saw(P[K], Dt) * (K == 3 ? 0.4 : 1.0);
				L += S * (K % 2 ? 0.7 : 1.0);
				R += S * (K % 2 ? 1.0 : 0.7);
			}
			ToneL.Run(L, 140.0 + 260.0 * SlowA, 1.4);
			ToneR.Run(R, 150.0 + 250.0 * SlowA, 1.4);
			BrownL = Clamp(BrownL * 0.995 + Rng.Signed() * 0.05, -1.0, 1.0);
			BrownR = Clamp(BrownR * 0.995 + Rng.Signed() * 0.05, -1.0, 1.0);
			AirL.Run(BrownL, 300.0 + 900.0 * SlowB, 2.0);
			AirR.Run(BrownR, 350.0 + 850.0 * SlowB, 2.0);
			B.L[I] = ToneL.Lp * 0.25 + AirL.Bp * 0.5;
			B.R[I] = ToneR.Lp * 0.25 + AirR.Bp * 0.5;
		}
		ApplyReverb(B, 0.9, 0.6, 0.8, 150.0);
		ApplyPump(B, 0.2, 0.2);
		return B;
	}

	// Arp: 16th-note chord tones over two octaves, through a filter that breathes across 4 bars, with a dotted-8th delay.
	FBuffer RenderArp(int Bars)
	{
		const size_t Loop = static_cast<size_t>(Bars) * SamplesPerBar;
		FBuffer B(2 * Loop);
		const int Pattern[16] = {0, 2, 4, 2, 3, 1, 2, 0, 0, 2, 4, 3, 2, 4, 1, 2};
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Bar = 0; Bar < Bars; ++Bar)
			{
				const FChord& C = ChordAtBar(Bar);
				const int Root = C.Root + 24;
				const int Tones[5] = {Root, Root + Third(C), Root + 7, Root + 12, Root + 12 + Third(C)};
				const int Base = (Pass * Bars + Bar) * StepsPerBar;
				for (int S = 0; S < StepsPerBar; ++S)
				{
					const size_t At = StepSample(Base + S);
					const double F = MidiHz(Tones[Pattern[S]]);
					const double LoopPhase = static_cast<double>(At % Loop) / static_cast<double>(Loop);
					const double Breath = 0.5 - 0.5 * std::cos(TwoPi * LoopPhase * (Bars / 4.0));
					FSvf Filter;
					double P = 0.0;
					const size_t N = static_cast<size_t>(SamplesPerStep * 0.9);
					for (size_t I = 0; I < N; ++I)
					{
						const double T = static_cast<double>(I) / SampleRate;
						const double Dt = F / SampleRate;
						P = std::fmod(P + Dt, 1.0);
						const double X = Square(P, Dt) * 0.6 + Saw(P, Dt) * 0.4;
						Filter.Run(X, 600.0 + 2600.0 * Breath * std::exp(-T / 0.05) + 400.0 * Breath, 3.0);
						const double Env = std::exp(-T / 0.07) * Clamp(static_cast<double>(N - I) / 120.0, 0.0, 1.0);
						const double V = Filter.Lp * Env * (S % 4 == 0 ? 0.5 : 0.38);
						B.Add(At + I, V, V);
					}
				}
			}
		}
		HighPass(B, 250.0);
		ApplyDelay(B, 3, 0.45, 0.55, 3000.0);
		ApplyReverb(B, 0.6, 0.5, 0.5, 300.0);
		ApplyPump(B, 0.35, 0.12);
		return B;
	}

	// Monophonic lead with glide: saw + square an octave down, detuned, driven, through a low-pass with a per-note envelope.
	void RenderLeadVoice(FBuffer& B, const std::vector<FNote>& Notes, int LoopSteps, double DriveAmount, double CutBase, double CutEnv,
		double Glide)
	{
		const size_t N = B.Size();
		std::vector<double> Pitch(N, -1.0), NoteStart(N, 0.0);
		std::vector<double> Velocity(N, 0.0);
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (const FNote& Note : Notes)
			{
				const size_t A = StepSample(Pass * LoopSteps + Note.Step);
				const size_t E = std::min(N, A + StepSample(Note.Length) - static_cast<size_t>(0.01 * SampleRate));
				for (size_t I = A; I < E; ++I)
				{
					Pitch[I] = Note.Note;
					NoteStart[I] = static_cast<double>(A);
					Velocity[I] = Note.Velocity;
				}
			}
		}
		double Current = Notes.empty() ? 60.0 : Notes.back().Note;
		double P1 = 0.0, P2 = 0.5, P3 = 0.0, Amp = 0.0;
		FSvf Filter;
		for (size_t I = 0; I < N; ++I)
		{
			const bool bOn = Pitch[I] >= 0.0;
			if (bOn) { Current += (Pitch[I] - Current) * Glide; }
			Amp += ((bOn ? Velocity[I] : 0.0) - Amp) * (bOn ? 0.01 : 0.0015);
			const double T = bOn ? (static_cast<double>(I) - NoteStart[I]) / SampleRate : 1.0;
			const double Vibrato = 0.12 * std::sin(TwoPi * 5.5 * static_cast<double>(I) / SampleRate) * Clamp((T - 0.25) / 0.3, 0.0, 1.0);
			const double F = MidiHz(Current + Vibrato);
			const double D1 = F * 1.003 / SampleRate, D2 = F * 0.997 / SampleRate, D3 = F * 0.5 / SampleRate;
			P1 = std::fmod(P1 + D1, 1.0);
			P2 = std::fmod(P2 + D2, 1.0);
			P3 = std::fmod(P3 + D3, 1.0);
			const double X = Saw(P1, D1) + Saw(P2, D2) + 0.5 * Square(P3, D3);
			Filter.Run(X, CutBase + CutEnv * std::exp(-T / 0.15), 1.6);
			const double V = Drive(Filter.Lp * 0.5, DriveAmount) * Amp;
			B.L[I] += V;
			B.R[I] += V;
		}
	}

	// Lead A: the hook. A long singing line over the progression. (Original melody.)
	FBuffer RenderLeadA(int Bars)
	{
		struct FSeg { int Len; int Note; };
		// Two bars each, 32 steps; Note 0 = rest.
		const std::vector<std::vector<FSeg>> Phrases = {
			{{4, 72}, {2, 68}, {2, 72}, {6, 77}, {2, 75}, {4, 72}, {4, 68}, {2, 67}, {2, 68}, {4, 0}},   // Fm
			{{4, 77}, {2, 80}, {2, 77}, {6, 73}, {2, 72}, {4, 73}, {4, 77}, {4, 75}, {4, 0}},            // Db
			{{6, 79}, {2, 77}, {4, 75}, {4, 70}, {4, 75}, {4, 79}, {6, 82}, {2, 0}},                     // Eb
			{{8, 76}, {4, 79}, {2, 77}, {2, 76}, {8, 72}, {4, 71}, {4, 0}},                               // C
			{{4, 72}, {2, 68}, {2, 72}, {6, 77}, {2, 79}, {4, 80}, {4, 79}, {4, 77}, {4, 0}},            // Fm
			{{4, 77}, {2, 80}, {2, 77}, {6, 73}, {2, 72}, {4, 73}, {4, 77}, {4, 75}, {4, 0}},            // Db
			{{4, 77}, {2, 73}, {2, 77}, {6, 82}, {2, 80}, {4, 77}, {4, 73}, {4, 72}, {4, 0}},            // Bbm
			{{4, 79}, {4, 76}, {4, 72}, {2, 76}, {2, 79}, {8, 84}, {2, 83}, {2, 79}, {4, 0}},            // C
		};
		std::vector<FNote> Notes;
		for (int Bar = 0; Bar < Bars; Bar += 2)
		{
			int Step = Bar * StepsPerBar;
			for (const FSeg& S : Phrases[(Bar / 2) % 8])
			{
				if (S.Note > 0) { Notes.push_back({Step, S.Len, static_cast<double>(S.Note), Step % 16 == 0 ? 1.0 : 0.85}); }
				Step += S.Len;
			}
		}
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		RenderLeadVoice(B, Notes, Bars * StepsPerBar, 1.8, 900.0, 2600.0, 0.004);
		HighPass(B, 200.0);
		ApplyDelay(B, 3, 0.38, 0.4, 3200.0);
		ApplyReverb(B, 0.75, 0.45, 0.7, 300.0);
		return B;
	}

	// Lead B: a hard, driven riff on the chord tones (rhythmic instead of melodic). (Original riff.)
	FBuffer RenderLeadB(int Bars)
	{
		struct FHit { int Step; int Len; int Degree; int Octave; };
		// Degree: 0 root, 1 third, 2 fifth. Two bars.
		const FHit Riff[] = {
			{0, 3, 0, 1}, {3, 2, 0, 1}, {6, 2, 1, 1}, {8, 4, 2, 1}, {12, 2, 0, 2}, {14, 2, 2, 1},
			{16, 3, 0, 1}, {19, 2, 0, 1}, {22, 2, 1, 1}, {24, 2, 2, 1}, {26, 2, 1, 1}, {28, 4, 2, 0},
		};
		std::vector<FNote> Notes;
		for (int Bar = 0; Bar < Bars; Bar += 2)
		{
			const FChord& C = ChordAtBar(Bar);
			const int Root = C.Root + 24;
			for (const FHit& H : Riff)
			{
				const int Offset = H.Degree == 0 ? 0 : (H.Degree == 1 ? Third(C) : 7);
				Notes.push_back({Bar * StepsPerBar + H.Step, H.Len, static_cast<double>(Root - 12 + Offset + 12 * H.Octave),
					H.Step % 8 == 0 ? 1.0 : 0.8});
			}
		}
		FBuffer B(static_cast<size_t>(2 * Bars) * SamplesPerBar);
		RenderLeadVoice(B, Notes, Bars * StepsPerBar, 4.0, 500.0, 3800.0, 0.02);
		HighPass(B, 150.0);
		ApplyDelay(B, 3, 0.3, 0.3, 2500.0);
		ApplyReverb(B, 0.5, 0.5, 0.45, 300.0);
		return B;
	}

	// ---- output -----------------------------------------------------------------------------------------------------------

	// Keeps the second pass of a stem rendered twice: [Loop, 2 Loop). Stems that never stop sounding (free-running oscillators,
	// noise) render a little past the second pass; that extra tail is equal-power crossfaded into the loop start, so the wrap from
	// the last sample back to the first stays continuous.
	FBuffer SecondPass(const FBuffer& Twice, size_t Loop)
	{
		FBuffer Out(Loop);
		std::copy(Twice.L.begin() + static_cast<long>(Loop), Twice.L.begin() + static_cast<long>(2 * Loop), Out.L.begin());
		std::copy(Twice.R.begin() + static_cast<long>(Loop), Twice.R.begin() + static_cast<long>(2 * Loop), Out.R.begin());
		const size_t Fade = Twice.Size() > 2 * Loop ? Twice.Size() - 2 * Loop : 0;
		for (size_t I = 0; I < Fade; ++I)
		{
			const double G = 0.5 * Pi * static_cast<double>(I) / static_cast<double>(Fade);
			Out.L[I] = Out.L[I] * std::sin(G) + Twice.L[2 * Loop + I] * std::cos(G);
			Out.R[I] = Out.R[I] * std::sin(G) + Twice.R[2 * Loop + I] * std::cos(G);
		}
		return Out;
	}

	double Rms(const FBuffer& B)
	{
		double Sum = 0.0;
		for (size_t I = 0; I < B.Size(); ++I) { Sum += B.L[I] * B.L[I] + B.R[I] * B.R[I]; }
		return std::sqrt(Sum / (2.0 * static_cast<double>(std::max<size_t>(1, B.Size()))));
	}

	double Peak(const FBuffer& B)
	{
		double P = 0.0;
		for (size_t I = 0; I < B.Size(); ++I) { P = std::max({P, std::fabs(B.L[I]), std::fabs(B.R[I])}); }
		return P;
	}

	bool WriteWav(const std::string& Path, const FBuffer& B, bool bMono)
	{
		FILE* F = std::fopen(Path.c_str(), "wb");
		if (!F) { return false; }
		const uint16_t Channels = bMono ? 1 : 2;
		const uint32_t DataBytes = static_cast<uint32_t>(B.Size() * Channels * 2);
		auto U32 = [F](uint32_t V) { std::fwrite(&V, 4, 1, F); };
		auto U16 = [F](uint16_t V) { std::fwrite(&V, 2, 1, F); };
		std::fwrite("RIFF", 1, 4, F); U32(36 + DataBytes); std::fwrite("WAVE", 1, 4, F);
		std::fwrite("fmt ", 1, 4, F); U32(16); U16(1); U16(Channels); U32(SampleRate); U32(SampleRate * Channels * 2u);
		U16(static_cast<uint16_t>(Channels * 2)); U16(16);
		std::fwrite("data", 1, 4, F); U32(DataBytes);
		std::vector<int16_t> Pcm;
		Pcm.reserve(B.Size() * Channels);
		auto Q = [](double X) { return static_cast<int16_t>(std::lround(Clamp(X, -1.0, 1.0) * 32767.0)); };
		for (size_t I = 0; I < B.Size(); ++I)
		{
			if (bMono) { Pcm.push_back(Q(0.5 * (B.L[I] + B.R[I]))); }
			else { Pcm.push_back(Q(B.L[I])); Pcm.push_back(Q(B.R[I])); }
		}
		std::fwrite(Pcm.data(), 2, Pcm.size(), F);
		return std::fclose(F) == 0;
	}

	// Plays stems along a bar timeline: Parts[i] = stems sounding in bar i (looped stems keep their phase from bar 0, as in game).
	FBuffer Arrange(const std::vector<FStem>& Stems, const std::vector<std::vector<std::string>>& Parts)
	{
		FBuffer Out(Parts.size() * SamplesPerBar);
		const size_t Fade = SampleRate / 50;
		for (size_t Bar = 0; Bar < Parts.size(); ++Bar)
		{
			for (const FStem& S : Stems)
			{
				const bool bOn = std::find(Parts[Bar].begin(), Parts[Bar].end(), S.Name) != Parts[Bar].end();
				if (!bOn) { continue; }
				const bool bWasOn = Bar > 0 && std::find(Parts[Bar - 1].begin(), Parts[Bar - 1].end(), S.Name) != Parts[Bar - 1].end();
				const bool bStaysOn = Bar + 1 < Parts.size() && std::find(Parts[Bar + 1].begin(), Parts[Bar + 1].end(), S.Name) != Parts[Bar + 1].end();
				const size_t Len = S.Audio.Size();
				for (size_t I = 0; I < SamplesPerBar; ++I)
				{
					const size_t T = Bar * SamplesPerBar + I;
					double G = 1.0;
					if (!bWasOn) { G = std::min(G, static_cast<double>(I) / Fade); }
					if (!bStaysOn) { G = std::min(G, static_cast<double>(SamplesPerBar - I) / Fade); }
					Out.L[T] += S.Audio.L[T % Len] * std::min(1.0, G);
					Out.R[T] += S.Audio.R[T % Len] * std::min(1.0, G);
				}
			}
		}
		return Out;
	}

	std::vector<std::vector<std::string>> Section(int Bars, std::vector<std::string> Stems)
	{
		return std::vector<std::vector<std::string>>(static_cast<size_t>(Bars), Stems);
	}
	void Append(std::vector<std::vector<std::string>>& A, const std::vector<std::vector<std::string>>& B) { A.insert(A.end(), B.begin(), B.end()); }
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: %s <out dir>\n", Argv[0]);
		return 2;
	}
	const std::string Dir = Argv[1];

	// Name, bars, mono, target RMS (dBFS) of the stem alone: the mix balance.
	std::vector<FStem> Stems = {
		{"Kick", 8, true, -13.0, FBuffer()},
		{"Hats", 8, false, -27.0, FBuffer()},
		{"Perc", 8, false, -24.0, FBuffer()},
		{"BassA", 16, true, -17.0, FBuffer()},
		{"BassB", 16, true, -17.0, FBuffer()},
		{"Sub", 16, true, -18.0, FBuffer()},
		{"Pad", 32, false, -23.0, FBuffer()},
		{"Drone", 32, false, -25.0, FBuffer()},
		{"Arp", 16, false, -25.0, FBuffer()},
		{"LeadA", 16, false, -21.0, FBuffer()},
		{"LeadB", 16, false, -21.0, FBuffer()},
	};
	for (FStem& S : Stems)
	{
		FBuffer Twice;
		if (S.Name == "Kick") { Twice = RenderKick(S.Bars); }
		else if (S.Name == "Hats") { Twice = RenderHats(S.Bars); }
		else if (S.Name == "Perc") { Twice = RenderPerc(S.Bars); }
		else if (S.Name == "BassA") { Twice = RenderBassA(S.Bars); }
		else if (S.Name == "BassB") { Twice = RenderBassB(S.Bars); }
		else if (S.Name == "Sub") { Twice = RenderSub(S.Bars); }
		else if (S.Name == "Pad") { Twice = RenderPad(S.Bars); }
		else if (S.Name == "Drone") { Twice = RenderDrone(S.Bars); }
		else if (S.Name == "Arp") { Twice = RenderArp(S.Bars); }
		else if (S.Name == "LeadA") { Twice = RenderLeadA(S.Bars); }
		else if (S.Name == "LeadB") { Twice = RenderLeadB(S.Bars); }
		S.Audio = SecondPass(Twice, static_cast<size_t>(S.Bars) * SamplesPerBar);
		if (S.bMono)
		{
			for (size_t I = 0; I < S.Audio.Size(); ++I) { S.Audio.L[I] = S.Audio.R[I] = 0.5 * (S.Audio.L[I] + S.Audio.R[I]); }
		}
		const double Gain = Db(S.TargetRmsDb) / std::max(1e-9, Rms(S.Audio));
		for (size_t I = 0; I < S.Audio.Size(); ++I) { S.Audio.L[I] *= Gain; S.Audio.R[I] *= Gain; }
	}

	// The loudest thing the game can play: every layer at once with the louder variant of each pair (BassA/B, LeadA/B).
	// One master gain for all stems keeps that under -1 dBFS, so the stems stay balanced against each other.
	double Worst = 0.0;
	for (const char* Bass : {"BassA", "BassB"})
	{
		for (const char* Lead : {"LeadA", "LeadB"})
		{
			std::vector<std::vector<std::string>> All = Section(32, {"Kick", "Hats", "Perc", "Sub", "Pad", "Drone", "Arp", Bass, Lead});
			Worst = std::max(Worst, Peak(Arrange(Stems, All)));
		}
	}
	const double Master = Db(-1.0) / Worst;
	for (FStem& S : Stems)
	{
		for (size_t I = 0; I < S.Audio.Size(); ++I) { S.Audio.L[I] *= Master; S.Audio.R[I] *= Master; }
	}

	bool bOk = true;
	std::printf("%-6s %5s %8s %9s %9s  file\n", "stem", "bars", "seconds", "rms dB", "peak dB");
	for (const FStem& S : Stems)
	{
		const std::string Path = Dir + "/MUS_Club_" + S.Name + ".wav";
		bOk &= WriteWav(Path, S.Audio, S.bMono);
		std::printf("%-6s %5d %8.2f %9.1f %9.1f  %s\n", S.Name.c_str(), S.Bars, static_cast<double>(S.Audio.Size()) / SampleRate,
			20.0 * std::log10(Rms(S.Audio)), 20.0 * std::log10(Peak(S.Audio)), Path.c_str());
	}

	// Previews: what each intensity tier sounds like, and a demo run through all of them, as the game would mix them.
	const std::vector<std::string> Prowl = {"Drone", "Pad", "Sub", "Hats"};
	const std::vector<std::string> Hunt = {"Drone", "Pad", "Kick", "Hats", "BassA", "Arp"};
	const std::vector<std::string> Fight = {"Pad", "Kick", "Hats", "Perc", "BassA", "Arp", "LeadA"};
	std::vector<std::vector<std::string>> Demo;
	Append(Demo, Section(16, Prowl));
	Append(Demo, Section(16, Hunt));
	Append(Demo, Section(16, Fight));
	Append(Demo, Section(4, {"Pad", "Hats", "Sub", "Arp"})); // breakdown
	Append(Demo, Section(16, {"Pad", "Kick", "Hats", "Perc", "BassB", "Arp", "LeadB"}));
	Append(Demo, Section(8, Hunt));
	Append(Demo, Section(8, Prowl));
	const struct { const char* Name; std::vector<std::vector<std::string>> Parts; } Previews[] = {
		{"Preview_Prowl", Section(16, Prowl)},
		{"Preview_Hunt", Section(16, Hunt)},
		{"Preview_Fight", Section(16, Fight)},
		{"Preview_Demo", Demo},
	};
	for (const auto& P : Previews)
	{
		const FBuffer Mix = Arrange(Stems, P.Parts);
		const std::string Path = Dir + "/" + P.Name + ".wav";
		bOk &= WriteWav(Path, Mix, false);
		std::printf("%-14s %6.1f s, peak %5.1f dB  %s\n", P.Name, static_cast<double>(Mix.Size()) / SampleRate, 20.0 * std::log10(Peak(Mix)),
			Path.c_str());
	}
	std::printf("master gain %.3f (worst-case full mix peak before it: %.2f)\n", Master, Worst);
	return bOk ? 0 : 1;
}
