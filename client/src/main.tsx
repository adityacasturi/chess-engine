import React from 'react';
import ReactDOM from 'react-dom/client';
import { Chess } from 'chess.js';
import type { Square } from 'chess.js';
import { Chessboard } from 'react-chessboard';
import type {
  PieceDropHandlerArgs,
  PieceHandlerArgs,
  SquareHandlerArgs
} from 'react-chessboard/dist/types';
import './styles.css';

type EngineResult = {
  accepted: boolean;
  fen: string;
  engineMove: string;
  status: string;
  score: number;
  nodes: number;
  timeMs: number;
  error?: string;
};

type MoveRecord = {
  human: string;
  engine: string;
};

const GAME_OVER_STATUSES = new Set(['checkmate', 'stalemate', 'draw']);
const MATE_SCORE = 100000;

async function postJson<T>(url: string, body?: unknown): Promise<T> {
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined
  });
  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.error ?? 'Request failed');
  }
  return data;
}

function statusFor(game: Chess): string {
  if (game.isCheckmate()) return 'checkmate';
  if (game.isStalemate()) return 'stalemate';
  if (game.isDraw()) return 'draw';
  if (game.isCheck()) return 'check';
  return 'ongoing';
}

function messageFor(game: Chess): string {
  const status = statusFor(game);
  if (status === 'checkmate') return game.turn() === 'w' ? 'Black wins by checkmate' : 'White wins by checkmate';
  if (status === 'stalemate') return 'Stalemate';
  if (status === 'draw') return 'Draw';
  if (status === 'check') return `${game.turn() === 'w' ? 'White' : 'Black'} to move, check`;
  return `${game.turn() === 'w' ? 'White' : 'Black'} to move`;
}

function needsPromotion(pieceType: string | undefined, sourceSquare: string, targetSquare: string): boolean {
  const isPawn = pieceType?.toLowerCase().includes('p') ?? false;
  const whitePromotion = sourceSquare[1] === '7' && targetSquare[1] === '8';
  const blackPromotion = sourceSquare[1] === '2' && targetSquare[1] === '1';
  return isPawn && (whitePromotion || blackPromotion);
}

function parseUciMove(move: string) {
  return {
    from: move.slice(0, 2) as Square,
    to: move.slice(2, 4) as Square,
    promotion: move.slice(4, 5) || undefined
  };
}

function uciMove(from: string, to: string, promotion?: string): string {
  return `${from}${to}${promotion ?? ''}`;
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function formatEval(score: number): string {
  if (Math.abs(score) >= 90000) {
    const plyDistance = Math.max(1, MATE_SCORE - Math.abs(score));
    const moveDistance = Math.max(1, Math.ceil(plyDistance / 2));
    return `${score > 0 ? '+' : '-'}M${moveDistance}`;
  }
  const pawns = score / 100;
  return `${pawns >= 0 ? '+' : ''}${pawns.toFixed(1)}`;
}

function whiteEvalShare(score: number): number {
  if (Math.abs(score) >= 90000) {
    return score > 0 ? 100 : 0;
  }
  return clamp(50 + score / 18, 4, 96);
}

function App() {
  const initialGame = React.useMemo(() => new Chess(), []);
  const [fen, setFen] = React.useState(initialGame.fen());
  const [status, setStatus] = React.useState(statusFor(initialGame));
  const [depth, setDepth] = React.useState(3);
  const [history, setHistory] = React.useState<MoveRecord[]>([]);
  const [pending, setPending] = React.useState(false);
  const [lastStats, setLastStats] = React.useState({ score: 0, nodes: 0, timeMs: 0 });
  const [message, setMessage] = React.useState(messageFor(initialGame));
  const [activeSquare, setActiveSquare] = React.useState<string | null>(null);
  const [legalSquares, setLegalSquares] = React.useState<string[]>([]);
  const sideToMove = fen.includes(' w ') ? 'White' : 'Black';
  const gameLocked = pending || sideToMove !== 'White' || GAME_OVER_STATUSES.has(status);
  const whiteEvalPercent = whiteEvalShare(lastStats.score);
  const evalLabel = formatEval(lastStats.score);

  const clearHighlights = React.useCallback(() => {
    setActiveSquare(null);
    setLegalSquares([]);
  }, []);

  const showLegalMoves = React.useCallback(
    (square: string | null, pieceType?: string) => {
      if (!square || pending || sideToMove !== 'White' || !pieceType?.startsWith('w')) {
        clearHighlights();
        return;
      }

      const game = new Chess(fen);
      const moves = game.moves({ square: square as Square, verbose: true });
      setActiveSquare(square);
      setLegalSquares(moves.map((move) => move.to));
    },
    [clearHighlights, fen, pending, sideToMove]
  );

  const resetGame = React.useCallback(() => {
    const game = new Chess();
    setFen(game.fen());
    setStatus(statusFor(game));
    setHistory([]);
    setPending(false);
    setLastStats({ score: 0, nodes: 0, timeMs: 0 });
    setMessage(messageFor(game));
    clearHighlights();
  }, [clearHighlights]);

  const applyEngineMove = React.useCallback((baseFen: string, engineMove: string) => {
    const game = new Chess(baseFen);
    try {
      if (engineMove) {
        game.move(parseUciMove(engineMove));
      }
    } catch {
      setMessage(`Engine returned an illegal move: ${engineMove}`);
      return;
    }
    setFen(game.fen());
    setStatus(statusFor(game));
    setMessage(messageFor(game));
  }, []);

  const requestEngineMove = React.useCallback(
    async (humanFen: string, humanMove: string) => {
      setPending(true);
      setMessage('Engine thinking...');
      try {
        const result = await postJson<EngineResult>('/api/engine-move', { fen: humanFen, depth });
        setLastStats({ score: result.score, nodes: result.nodes, timeMs: result.timeMs });
        applyEngineMove(humanFen, result.engineMove);
        setHistory((current) => {
          const next = [...current];
          const last = next[next.length - 1];
          if (last?.human === humanMove && last.engine === '...') {
            next[next.length - 1] = { human: humanMove, engine: result.engineMove || 'game over' };
          }
          return next;
        });
      } catch (error) {
        setMessage(error instanceof Error ? error.message : 'Engine request failed');
        setHistory((current) => {
          const next = [...current];
          const last = next[next.length - 1];
          if (last?.human === humanMove && last.engine === '...') {
            next[next.length - 1] = { human: humanMove, engine: 'error' };
          }
          return next;
        });
      } finally {
        setPending(false);
      }
    },
    [applyEngineMove, depth]
  );

  const submitMove = React.useCallback(
    (from: string, to: string, promotion = 'q') => {
      if (gameLocked) {
        return false;
      }

      const game = new Chess(fen);
      let move;
      try {
        move = game.move({ from: from as Square, to: to as Square, promotion });
      } catch {
        move = null;
      }
      if (!move) {
        setMessage('Illegal move');
        return false;
      }

      const humanMove = uciMove(move.from, move.to, move.promotion);
      const humanFen = game.fen();
      const humanStatus = statusFor(game);

      setFen(humanFen);
      setStatus(humanStatus);
      setMessage(messageFor(game));
      setHistory((current) => [...current, { human: humanMove, engine: '...' }]);
      clearHighlights();

      if (GAME_OVER_STATUSES.has(humanStatus)) {
        setHistory((current) => {
          const next = [...current];
          next[next.length - 1] = { human: humanMove, engine: 'game over' };
          return next;
        });
        return true;
      }

      void requestEngineMove(humanFen, humanMove);
      return true;
    },
    [clearHighlights, fen, gameLocked, requestEngineMove]
  );

  const onPieceDrop = React.useCallback(
    ({ sourceSquare, targetSquare, piece }: PieceDropHandlerArgs) => {
      clearHighlights();
      if (!targetSquare) return false;
      const promotion = needsPromotion(piece?.pieceType, sourceSquare, targetSquare)
        ? window.prompt('Promote to q, r, b, or n', 'q')?.toLowerCase() ?? 'q'
        : '';
      const safePromotion = ['q', 'r', 'b', 'n'].includes(promotion) ? promotion : 'q';
      return submitMove(sourceSquare, targetSquare, promotion ? safePromotion : undefined);
    },
    [clearHighlights, submitMove]
  );

  const canDragPiece = React.useCallback(
    ({ piece }: PieceHandlerArgs) => !gameLocked && piece.pieceType.startsWith('w'),
    [gameLocked]
  );

  const onPieceDrag = React.useCallback(
    ({ piece, square }: PieceHandlerArgs) => {
      showLegalMoves(square, piece.pieceType);
    },
    [showLegalMoves]
  );

  const onPieceClick = React.useCallback(
    ({ piece, square }: PieceHandlerArgs) => {
      showLegalMoves(square, piece.pieceType);
    },
    [showLegalMoves]
  );

  const onSquareClick = React.useCallback(
    ({ piece, square }: SquareHandlerArgs) => {
      if (activeSquare && legalSquares.includes(square)) {
        const selectedPiece = new Chess(fen).get(activeSquare as Square);
        const promotion = needsPromotion(selectedPiece?.type, activeSquare, square)
          ? window.prompt('Promote to q, r, b, or n', 'q')?.toLowerCase() ?? 'q'
          : '';
        const safePromotion = ['q', 'r', 'b', 'n'].includes(promotion) ? promotion : 'q';
        submitMove(activeSquare, square, promotion ? safePromotion : undefined);
        return;
      }

      if (piece?.pieceType.startsWith('w')) {
        showLegalMoves(square, piece.pieceType);
      } else {
        clearHighlights();
      }
    },
    [activeSquare, clearHighlights, fen, legalSquares, showLegalMoves, submitMove]
  );

  const squareStyles = React.useMemo(
    () =>
      legalSquares.reduce<Record<string, React.CSSProperties>>(
        (styles, square) => {
          styles[square] = {
            background:
              'radial-gradient(circle at center, rgba(17, 24, 39, 0.28) 0 18%, transparent 19%)'
          };
          return styles;
        },
        activeSquare
          ? {
              [activeSquare]: {
                boxShadow: 'inset 0 0 0 4px rgba(17, 24, 39, 0.34)'
              }
            }
          : {}
      ),
    [activeSquare, legalSquares]
  );

  return (
    <main className="app-shell">
      <section className="play-surface" aria-label="Chess board">
        <div className="board-row">
          <div className="eval-bar" aria-label={`Evaluation ${evalLabel}`}>
            <div className="eval-black" style={{ height: `${100 - whiteEvalPercent}%` }} />
            <div className="eval-white" style={{ height: `${whiteEvalPercent}%` }} />
            <span className="eval-label">{evalLabel}</span>
          </div>
          <div className="board-zone">
            <Chessboard
              options={{
                position: fen,
                boardOrientation: 'white',
                onPieceDrop,
                onPieceDrag,
                onPieceClick,
                onSquareClick,
                canDragPiece,
                squareStyles,
                animationDurationInMs: 160,
                allowDragging: !gameLocked,
                boardStyle: {
                  borderRadius: '3px',
                  boxShadow: '0 18px 55px rgba(18, 21, 17, 0.18)'
                },
                lightSquareStyle: { backgroundColor: '#e7ddc7' },
                darkSquareStyle: { backgroundColor: '#637761' },
                dropSquareStyle: { boxShadow: 'inset 0 0 0 4px rgba(21, 23, 19, 0.34)' },
                darkSquareNotationStyle: { color: '#e7ddc7', fontFamily: 'inherit' },
                lightSquareNotationStyle: { color: '#637761', fontFamily: 'inherit' }
              }}
            />
          </div>
        </div>
      </section>

      <aside className="panel" aria-label="Game controls">
        <p className="status">{message}</p>

        <div className="controls">
          <label htmlFor="depth">depth</label>
          <input
            id="depth"
            type="range"
            min="1"
            max="5"
            value={depth}
            onChange={(event) => setDepth(Number(event.target.value))}
            disabled={pending}
          />
          <output>{depth}</output>
        </div>

        <div className="actions">
          <button className="primary-action" type="button" onClick={resetGame} disabled={pending}>
            reset
          </button>
        </div>

        <dl className="stats">
          <div>
            <dt>status</dt>
            <dd>{status}</dd>
          </div>
          <div>
            <dt>eval</dt>
            <dd>{evalLabel}</dd>
          </div>
          <div>
            <dt>nodes</dt>
            <dd>{lastStats.nodes.toLocaleString()}</dd>
          </div>
          <div>
            <dt>time</dt>
            <dd>{lastStats.timeMs}ms</dd>
          </div>
        </dl>

        <div className="history">
          <h2>moves</h2>
          {history.length === 0 ? (
            <p className="empty-history">no moves yet</p>
          ) : (
            <ol>
              {history.map((record, index) => (
                <li key={`${record.human}-${index}`}>
                  <span>{String(index + 1).padStart(2, '0')}</span>
                  <span>{record.human}</span>
                  <span>{record.engine}</span>
                </li>
              ))}
            </ol>
          )}
        </div>
      </aside>
    </main>
  );
}

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
