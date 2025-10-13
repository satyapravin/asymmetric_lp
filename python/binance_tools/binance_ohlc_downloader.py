#!/usr/bin/env python3
"""
Binance OHLC Downloader
Fetches Binance spot aggTrades for ETHUSDC and builds 5-second OHLC bars.
Saves CSV with columns: timestamp,open,high,low,close,volume

Note: Uses public endpoints; respects basic rate limits with small sleeps.
"""
import time
import math
import argparse
from datetime import datetime, timezone
from typing import List, Dict
import requests
import pandas as pd

BASE_URL = "https://api.binance.com"


def ms(dt: datetime) -> int:
    return int(dt.replace(tzinfo=timezone.utc).timestamp() * 1000)


def iso(ts_ms: int) -> str:
    return datetime.utcfromtimestamp(ts_ms / 1000).strftime('%Y-%m-%d %H:%M:%S')


def fetch_agg_trades(symbol: str, start_ms: int, end_ms: int, window_ms: int = 30 * 60 * 1000,
                     per_req_sleep: float = 0.05) -> List[Dict]:
    """
    Fetch aggTrades for [start_ms, end_ms] inclusive, iterating within windows.
    Handles Binance 1000-record limit by advancing via last trade time.
    """
    all_trades: List[Dict] = []
    cur = start_ms
    while cur <= end_ms:
        w_end = min(cur + window_ms - 1, end_ms)
        while True:
            params = {
                'symbol': symbol,
                'startTime': cur,
                'endTime': w_end,
                'limit': 1000,
            }
            r = requests.get(f"{BASE_URL}/api/v3/aggTrades", params=params, timeout=30)
            r.raise_for_status()
            data = r.json()
            if not data:
                break
            all_trades.extend(data)
            last_t = data[-1]['T']
            # Advance; if last_t didn't move, break to avoid infinite loop
            if last_t <= cur:
                break
            cur = last_t + 1
            if cur > w_end:
                break
            time.sleep(per_req_sleep)
        cur = w_end + 1
        time.sleep(per_req_sleep)
    return all_trades


def build_5s_ohlc(trades: List[Dict]) -> pd.DataFrame:
    if not trades:
        return pd.DataFrame(columns=['timestamp', 'open', 'high', 'low', 'close', 'volume'])
    # Build DataFrame of price/time/size (quote volume in USDC)
    df = pd.DataFrame([{'t': t['T'], 'p': float(t['p']), 'q': float(t['q'])} for t in trades])
    # Binance aggTrades: p in quote/base? p is price (quote/base), q is quantity (base asset). For ETHUSDC, quote=USDC.
    # Quote volume per trade ~ p * q (USDC)
    df['bucket'] = (df['t'] // 5000) * 5000
    grouped = df.groupby('bucket', as_index=False)
    # Open/Close from first/last price
    o = grouped.first()[['bucket', 'p']].rename(columns={'p': 'open'})
    c = grouped.last()[['bucket', 'p']].rename(columns={'p': 'close'})
    # High/Low as series with explicit names
    h = grouped['p'].max()
    h.name = 'high'
    l = grouped['p'].min()
    l.name = 'low'
    # Volume as quote volume sum per bucket
    v = grouped.apply(lambda g: (g['p'] * g['q']).sum())
    v.name = 'volume'

    # Merge all
    out = o.merge(h, left_on='bucket', right_index=True)
    out = out.merge(l, left_on='bucket', right_index=True)
    out = out.merge(c, on='bucket')
    out = out.merge(v, left_on='bucket', right_index=True)
    out['timestamp'] = out['bucket'].apply(lambda x: iso(int(x)))
    out = out[['timestamp', 'open', 'high', 'low', 'close', 'volume']]
    # Sort by time
    out = out.sort_values('timestamp').reset_index(drop=True)
    return out


def main():
    ap = argparse.ArgumentParser(description='Binance 5s OHLC downloader (ETHUSDC)')
    ap.add_argument('--start', required=True, help='Start time UTC (YYYY-MM-DD HH:MM:SS)')
    ap.add_argument('--end', required=True, help='End time UTC (YYYY-MM-DD HH:MM:SS)')
    ap.add_argument('--symbol', default='ETHUSDC', help='Binance symbol (default ETHUSDC)')
    ap.add_argument('--output', required=True, help='Output CSV')
    args = ap.parse_args()

    start_dt = datetime.strptime(args.start, '%Y-%m-%d %H:%M:%S').replace(tzinfo=timezone.utc)
    end_dt = datetime.strptime(args.end, '%Y-%m-%d %H:%M:%S').replace(tzinfo=timezone.utc)
    start_ms = ms(start_dt)
    end_ms = ms(end_dt)

    print(f"Fetching aggTrades for {args.symbol} from {start_dt} to {end_dt} ...")
    trades = fetch_agg_trades(args.symbol, start_ms, end_ms)
    print(f"Fetched {len(trades)} aggTrades")

    print("Building 5s OHLC ...")
    ohlc = build_5s_ohlc(trades)
    # Save raw Binance (USDC per ETH)
    raw_out = args.output
    ohlc.to_csv(raw_out, index=False)
    print(f"Saved Binance OHLC (USDC/ETH) to {raw_out} with {len(ohlc)} rows")

    # Also produce inverted ETH/USDC file
    inv = ohlc.copy()
    for col in ['open', 'high', 'low', 'close']:
        inv[col] = 1.0 / inv[col]
    high = inv['high'].copy(); low = inv['low'].copy(); inv['high'] = low; inv['low'] = high
    inv[['open', 'high', 'low', 'close']] = inv[['open','high','low','close']].astype(float).round(12)
    inv_out = raw_out.replace('.csv', '_fixed.csv')
    inv.to_csv(inv_out, index=False)
    print(f"Saved inverted OHLC (ETH/USDC) to {inv_out} with {len(inv)} rows")

    return 0


if __name__ == '__main__':
    raise SystemExit(main())


