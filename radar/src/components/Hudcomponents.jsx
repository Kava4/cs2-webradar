import React from 'react';

// NEW FEATURES 6, 14, 15: Team Statistics HUD
const TeamStatsHUD = ({ data }) => {
  if (!data) return null;

  const aliveCount = data.m_alive_count || { t: 0, ct: 0 };
  const teamEconomy = data.m_team_economy || { t: 0, ct: 0 };
  const utilityCount = data.m_utility_count || { t: 0, ct: 0 };
  const localTeam = data.m_local_team || 2;

  const formatMoney = (amount) => {
    return `$${amount.toLocaleString()}`;
  };

  return (
    <div className="fixed top-4 left-1/2 -translate-x-1/2 z-[90] flex gap-4">
      {/* Terrorists Stats */}
      <div 
        className="px-4 py-2 rounded-xl backdrop-blur-xl flex items-center gap-3 min-w-[200px]"
        style={{
          background: 'linear-gradient(135deg, rgba(255,107,53,0.15), rgba(247,147,30,0.08))',
          border: localTeam === 2 ? '2px solid #FF6B35' : '1px solid rgba(255,107,53,0.3)',
          boxShadow: '0 4px 16px rgba(0,0,0,0.3)',
        }}
      >
        <div className="w-8 h-8 rounded-full bg-gradient-to-br from-orange-500 to-red-500 flex items-center justify-center text-white font-bold">
          T
        </div>
        <div className="flex-1 space-y-1">
          {/* NEW FEATURE 6: Alive Count */}
          <div className="flex items-center justify-between">
            <span className="text-orange-400 text-xs font-semibold">Alive</span>
            <span className="text-white font-bold text-sm">{aliveCount.t}/5</span>
          </div>
          
          {/* NEW FEATURE 14: Team Economy */}
          <div className="flex items-center justify-between">
            <span className="text-orange-400/70 text-[10px]">Economy</span>
            <span className="text-green-400 font-mono text-xs">{formatMoney(teamEconomy.t)}</span>
          </div>
          
          {/* NEW FEATURE 15: Utility Count */}
          <div className="flex items-center justify-between">
            <span className="text-orange-400/70 text-[10px]">Utilities</span>
            <span className="text-blue-400 font-mono text-xs">{utilityCount.t} 💣</span>
          </div>
        </div>
      </div>

      {/* Counter-Terrorists Stats */}
      <div 
        className="px-4 py-2 rounded-xl backdrop-blur-xl flex items-center gap-3 min-w-[200px]"
        style={{
          background: 'linear-gradient(135deg, rgba(74,144,226,0.15), rgba(53,122,189,0.08))',
          border: localTeam === 3 ? '2px solid #4A90E2' : '1px solid rgba(74,144,226,0.3)',
          boxShadow: '0 4px 16px rgba(0,0,0,0.3)',
        }}
      >
        <div className="w-8 h-8 rounded-full bg-gradient-to-br from-blue-500 to-cyan-500 flex items-center justify-center text-white font-bold">
          CT
        </div>
        <div className="flex-1 space-y-1">
          {/* NEW FEATURE 6: Alive Count */}
          <div className="flex items-center justify-between">
            <span className="text-blue-400 text-xs font-semibold">Alive</span>
            <span className="text-white font-bold text-sm">{aliveCount.ct}/5</span>
          </div>
          
          {/* NEW FEATURE 14: Team Economy */}
          <div className="flex items-center justify-between">
            <span className="text-blue-400/70 text-[10px]">Economy</span>
            <span className="text-green-400 font-mono text-xs">{formatMoney(teamEconomy.ct)}</span>
          </div>
          
          {/* NEW FEATURE 15: Utility Count */}
          <div className="flex items-center justify-between">
            <span className="text-blue-400/70 text-[10px]">Utilities</span>
            <span className="text-orange-400 font-mono text-xs">{utilityCount.ct} 💣</span>
          </div>
        </div>
      </div>
    </div>
  );
};

// NEW FEATURE 13: Round Timer HUD
const RoundTimerHUD = ({ data }) => {
  if (!data || !data.m_round_info) return null;

  const roundInfo = data.m_round_info;
  const currentTime = roundInfo.m_current_time || 0;
  
  // CS2 competitive round time is 1:55 (115 seconds)
  // This is simplified - ideally calculate from actual round start time
  const roundTimeLimit = 115;
  const elapsedTime = Math.min(currentTime % roundTimeLimit, roundTimeLimit);
  const remainingTime = Math.max(roundTimeLimit - elapsedTime, 0);
  
  const minutes = Math.floor(remainingTime / 60);
  const seconds = Math.floor(remainingTime % 60);
  const timeString = `${minutes}:${seconds.toString().padStart(2, '0')}`;

  const timePercentage = (remainingTime / roundTimeLimit) * 100;
  const isLowTime = remainingTime < 30;

  return (
    <div 
      className="fixed top-20 left-1/2 -translate-x-1/2 z-[90]"
    >
      <div 
        className={`px-6 py-3 rounded-2xl backdrop-blur-xl ${isLowTime ? 'animate-pulse' : ''}`}
        style={{
          background: isLowTime 
            ? 'linear-gradient(135deg, rgba(239,68,68,0.2), rgba(220,38,38,0.1))'
            : 'linear-gradient(135deg, rgba(255,255,255,0.1), rgba(255,255,255,0.05))',
          border: isLowTime ? '2px solid rgba(239,68,68,0.5)' : '1px solid rgba(255,255,255,0.2)',
          boxShadow: isLowTime 
            ? '0 0 20px rgba(239,68,68,0.3)' 
            : '0 4px 16px rgba(0,0,0,0.3)',
        }}
      >
        <div className="text-center">
          <div className="text-[10px] font-bold uppercase tracking-wider mb-1"
            style={{ color: isLowTime ? '#ef4444' : '#9ca3af' }}
          >
            Round Time
          </div>
          <div 
            className="text-3xl font-black font-mono"
            style={{ 
              color: isLowTime ? '#ef4444' : '#ffffff',
              textShadow: '0 2px 8px rgba(0,0,0,0.5)',
            }}
          >
            {timeString}
          </div>
        </div>
        
        {/* Time bar */}
        <div 
          className="mt-2 h-1 rounded-full overflow-hidden"
          style={{
            background: 'rgba(0,0,0,0.3)',
          }}
        >
          <div 
            className="h-full transition-all duration-1000 ease-linear"
            style={{
              width: `${timePercentage}%`,
              background: isLowTime 
                ? 'linear-gradient(90deg, #ef4444, #dc2626)'
                : 'linear-gradient(90deg, #22c55e, #16a34a)',
              boxShadow: isLowTime 
                ? '0 0 8px #ef4444'
                : '0 0 8px #22c55e',
            }}
          />
        </div>
      </div>
    </div>
  );
};

export { TeamStatsHUD, RoundTimerHUD };