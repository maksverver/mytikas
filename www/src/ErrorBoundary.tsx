import React, { type ErrorInfo, type ReactNode } from "react";

type Props = {
    fallback: ReactNode,
    children: ReactNode,
};

type State = {
    hasError: boolean,
};

export class ErrorBoundary extends React.Component<Props, State> {
    constructor(props: Props) {
        super(props);
        this.state = { hasError: false };
    }

    static getDerivedStateFromError(_error: Error) {
        return { hasError: true };
    }

    componentDidCatch(error: Error, info: ErrorInfo) {
        console.error('ErrorBoundary caught error', error);
        console.info('Component stack:', info.componentStack);

        const ownerStack = React.captureOwnerStack(); // only available in debug builds
        if (ownerStack != null) {
            console.info('Owner stack:', ownerStack);
        }
    }

    render() {
        return this.state.hasError ? this.props.fallback : this.props.children;
    }
}
